
### Kicking the tires of the CoreGuidelines lifetime checker - Jun 2018

This was just simply an exercise in coding up a number of simple use-after-free bugs and seeing if the msvc CoreGuidelines lifetime checker will catch them. Here we're using Visual Studio version 15.7.4 (up-to-date as of Jun 2018). Spoiler: It did catch "most" of them. 

### Not caught

Here are some it didn't catch:

###### snippet m1
```cpp
	{
		const std::string& str_cref = *std::make_unique<std::string>("some text"); // not caught
		std::string S2 = str_cref;
	}
```

This one was a little suprising as you'd think it'd be pretty much the simplest use-after-free bug involving `unique_ptr`. But this particular oversight isn't representative of the lifetime checker's general abilities.

Here's another one:

###### snippet m2
```cpp
	struct CH {
		static void foo1(const std::string& str_cref, std::shared_ptr<std::string>& shptr_ref) {
			shptr_ref = nullptr;
			std::cout << str_cref << std::endl; // not caught
		}
	};
	{
		auto shptr1 = std::make_shared<std::string>("hello");
		const std::string& str_cref = *shptr1;
		CH::foo1(str_cref, shptr1);
	}
```

This one was a little more tricky. Notably, if you replace the function call with the code inside the function, then the checker does catch it. Btw, it doesn't matter that the function is declared as a static member function, the result seems to be the same no matter what type of function it is. Also, you get the same result whether it's `shared_ptr` or `unique_ptr`. That seems to be the case generally.

###### snippet m3
```cpp
	struct CH {
		static void foo6(std::unique_ptr<std::string> str_uqptr) {
			auto str_ptr = str_uqptr.get();
			std::cout << *str_uqptr << std::endl;
		}
	};
	{
		auto str_uqptr = std::make_unique<std::string>("hello");
		auto str_ptr = str_uqptr.get();
		CH::foo6(std::move(str_uqptr));
		std::cout << *str_ptr << std::endl; // not caught
	}
```

In this one the `unique_ptr` was first moved, then destroyed, freeing the target while still referenced by the raw pointer. 

###### snippet m4
```cpp
	struct CJ {
		std::string m_string1 = "hello";
	};
	{
		std::vector<CJ> vec1;
		vec1.push_back(CJ());
		auto& a_ref = vec1.at(0);
		vec1.clear();
		std::cout << a_ref.m_string1 << std::endl; // not caught
	}
	{
		std::vector<CJ> vec1;
		vec1.push_back(CJ());
		auto& a_ref = vec1.at(0).m_string1;
		vec1.clear();
		std::cout << a_ref << std::endl; // not caught
	}
```

Here the reference is not to the vector element, but to a member of the vector element. The checker does catch the bug in the case when the reference is directly to the `vector` element. This one was a little bit surprising because there are other situations where the checker does catch the invalidation of a reference to a member of an object that has been deleted. Also note, this oversight is not specific to `vector`s. It also misses it in the case where the struct is held by a `shared_ptr` or `unique_ptr`.

###### snippet m5
```cpp
	struct CK {
		void foo1(std::string& str_ref) {
			std::cout << str_ref << std::endl;
			m_str_ptr = &str_ref;
		}
		void foo2() const {
			std::cout << *m_str_ptr << std::endl;
		}
		std::string* m_str_ptr = nullptr;
	};
	{
		CK k;
		{
			std::string str("hello");
			k.foo1(str);
			k.foo2();
		}
		k.foo2(); // not caught
		std::cout << *k.m_str_ptr << std::endl; // caught
	}
```

Here, as with the [second example](#snippet-m2), the violation is not caught when it occurs inside a function, namely, `foo2()`, but it is caught when essentially the same code is not "hidden" inside a function. 

###### snippet m6
```cpp
	struct CH {
		static std::string foo7(const std::string& str_cref) {
			std::cout << str_cref << std::endl;
			return str_cref;
		}
		static const std::string& foo8(const std::string& str_cref) {
			std::cout << str_cref << std::endl;
			return str_cref;
		}
	};
	{
		const auto& str_cref1 = CH::foo7(std::string("hello"));
		/* str_cref1 is a reference to a temporary value. It is valid here because the lifetime of the temporary object
		is extended by the compiler. */
		std::cout << str_cref1 << std::endl;

		const auto& str_cref2 = CH::foo8(std::string("hello")); // not caught
		/* str_cref2 is also (intended to be) a reference to a temporary value. But in this case the lifetime of the
		temporary object is not sufficiently extended, so the reference is not valid here. */
		std::cout << str_cref2 << std::endl;
	}
```

This one has to do with the intricacies of "lifetime extention of temporaries" by the compiler.

And here's one more:

###### snippet m7
```cpp
	struct CH {
		static auto foo3(const std::string& str1) {
			std::cout << str1 << std::endl;
			return &str1;
		}
	};
	{
		auto str_shptr = std::make_shared<std::string>("hello");
		auto str_ptr = CH::foo3(*str_shptr);
		str_shptr = nullptr;
		std::cout << *str_ptr << std::endl; // not caught
	}
```

Those were the ones the lifetime checker missed of the examples that came to mind. 

### Caught

But as mentioned, the checker caught "more" bugs than it missed. For example(s):

###### snippet c1
```cpp
	{
		auto shptr1 = std::make_shared<std::string>("hello");
		const std::string& str_cref = *shptr1;
		shptr1 = nullptr;
		std::cout << str_cref << std::endl; // caught
	}
```

###### snippet c2
```cpp
	struct CH {
		static auto foo3(const std::string& str1) {
			std::cout << str1 << std::endl;
			return &str1;
		}
		static auto foo4() {
			std::string str1("hello");
			return foo3(str1); // caught
		}
	};
	{
		auto str_ptr = CH::foo3(std::string("hello"));
		std::cout << *str_ptr << std::endl; // caught
	}
	{
		const std::string* str_ptr = nullptr;
		{
			auto str = std::string("hello");
			str_ptr = CH::foo3(str);
			std::cout << *str_ptr << std::endl;
		}
		std::cout << *str_ptr << std::endl; // caught
	}
```

###### snippet c3
```cpp
	struct CH {
		static auto foo5() {
			auto str_uqptr = std::make_unique<std::string>("hello");
			auto str_ptr = str_uqptr.get();
			struct str_ptr_wrapper_t {
				std::string* m_str_ptr = nullptr;
			};
			return str_ptr_wrapper_t{ str_ptr };
		}
	};
	{
		auto str_ptr = CH::foo5().m_str_ptr;
		std::cout << *str_ptr << std::endl; // caught
	}
```


### False positives

But there were also some notable false positives:

###### snippet f1
```cpp
	{
		struct CNode {
			std::string m_string = "hello";
			CNode* m_next = nullptr;
		};
		CNode node1;
		CNode node2;
		node1.m_next = &node2;
		CNode node3;
		node2.m_next = &node3;

		CNode* tmp_node_ptr1 = node1.m_next; // no complaint

		CNode* tmp_node_ptr2 = node1.m_next->m_next; // false positive
	}
```

###### snippet f2
```cpp
	{
		typedef const std::string const_string_t;
		struct CL {
			void set_target(const std::string& str_cref) {
				m_str_ptr = &str_cref;
			}
			const_string_t* m_str_ptr = nullptr;
		};
		std::string str("hello");
		CL l;
		l.set_target(str);
		std::cout << *(l.m_str_ptr) << std::endl; // false positive
	}
```

And one other thing to mention that we observed, but did not thoroughly investigate, is the phenomenon of the checkers simply failing to flag some non-compliant code in large source files (with a lot of other non-compliant code), while reliably flagging the same non-compliant code in smaller source files. We do not rule out the possibility that this is intended behavior.

So this version of the checker does not yet fully do the job, but it seems to be a lot of the way there.

Note that iterator, `string_view` or `span` reference types weren't tested. But those presumably aren't too fundamentally different from pointers?

