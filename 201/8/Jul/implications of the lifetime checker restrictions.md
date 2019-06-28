Jun 2019

### Implications of the Core Guidelines lifetime checker restrictions

When completed, the Core Guidelines lifetime checker will be able to ensure that C++ code has no use-after-free bugs by identifying/prohibiting any code that it cannot verify to be safe (with respect to use-after-free bugs). (From now on, we'll use the term "safe" to mean safe with respect to use-after-free bugs.) There will be lots of code that is actually safe, but that the lifetime checker won't be able to recognize as such. In broad terms, as a static analyzer, the lifetime checker should be able recognize as safe any code that is "structurally" safe. That is, code whose safety is solely a result of the placement of the pointer/reference elements and their target objects within (the structure of) the code.

In contrast, the safety of "logically" safe code is reliant on logical constraints on the value(s) of one or more expressions in the code.

This is a simple example of structurally safe code:

###### *snippet 1*
```cpp
typedef std::string* string_ptr_t; 	// unnecessary, but used for extra clarity

std::string s1("hello");
string_ptr_t string_ptr1 = &s1;
{
	std::string s2("world");
	string_ptr_t string_ptr2 = &s2;
	std::cout << string_ptr2->length();
}
std::cout << string_ptr1->length();
```

It's structurally safe in the sense that pointers `string_ptr1` and `string_ptr2` are guaranteed to always be valid due to where they and their target objects are structurally placed in the code. Now here's an example of logically safe code:

###### *snippet 2*
```cpp
typedef std::string* string_ptr_t;

std::string s1("hello");
string_ptr_t string_ptr1 = &s1;
const bool condition1 = foo1();
const bool condition2 = !condition1;
{
	std::string s2("world");

	/* Note that there are two strings but only one string pointer in this example. */

	if (condition1) {
		string_ptr1 = &s2;
	}
	std::cout << string_ptr1->length();
}
if (condition2) {
	std::cout << string_ptr1->length();
}
```

Note that in this example, if `condition1` and `condition2` were both true then the code would be unsafe. But there's a logical constraint ensuring that they cannot both be true. So this example is safe, but not structurally safe.

Now, static analyzers like the lifetime checker, cannot in general, recognize logically safe code as safe. But it can recognize some logically safe code as safe. In particular, if the only violations of structural safety occur via elements that are familiar to the checker and known to be logically safe. So for example, referencing an `std::vector` element via the `at()` member function would not qualify as structurally safe, but the `at()` member function is known to the checker to be logically safe. `std::optional::value()` would be another example of a familiar logically safe element. Note that all these logically safe elements incur some (usually small) run-time overhead.

The lifetime checker is familiar with a large enough set of logically safe elements that most algorithms can be implemented in a fairly efficient and straightforward manner. But there may still be some situations that are less optimal than we'd like. Consider this example:

###### *snippet 3*
```cpp
	{
		struct CState {
			typedef std::array<std::string, 10> type1;
			type1 m_data;
			int m_count = 0;
		};
		struct CSequence : public std::vector<CState> {
			typedef std::vector<CState> base_class;
			using base_class::base_class;

			void reset_sequence(CState& initial_state) {
				(*this).clear();
				//(*this).shrink_to_fit();
				(*this).push_back(initial_state);
				initial_state.m_count += 1;
			}
		};
		auto v1 = CSequence(2);
		auto v2 = CSequence(2);

		v2.reset_sequence(v1.at(0)); // structurally safe

		std::vector< CSequence > vectors{ v1, v2 };
		vectors.at(1).reset_sequence(vectors.at(0).at(0)); // logically, but not structurally safe

		std::vector< CSequence > vectors2{ v1, v2 };
		{
			auto tmp = std::move(vectors2.at(0).at(0));
			vectors2.at(1).reset_sequence(tmp); // structurally safe
			vectors2.at(0).at(0) = std::move(tmp); // but moves aren't always cheap
		}
	}
```

In this example there are three calls to the `reset_sequence()` member function. The first one is structurally safe and shouldn't upset the lifetime checker. The second one, however, is not structurally safe and it (or some generalized version of it) would not be accepted by the (eventual completed) lifetime checker.

While there does not yet (as of Jun 2018) seem to be any readily apparent advice on how to address this situation in the C++ world, the Rust language has had to deal with this kind of issue for a while and suggests something akin to the solution used with the third call. That is, temporarily moving the problematic value to a local variable before the call, and then moving the (modified) value back after the call. This solves the problem, but moves aren't always cheap or optimized out. This seems to be a [concern](https://www.reddit.com/r/rust/comments/8ts6b4/is_anyone_else_worried_of_performance_issues_due) in the Rust community as well.

Another example is the case when you have a dynamic container, like a list or whatever, whose elements contain (non-owning raw) pointers. Currently (Jun 2018) the lifetime checker doesn't address the safety issue here, but the eventual completed version will have to. Presumably, like the Rust compiler, it will require any target object of the raw pointers to structurally outlive the container itself. But that can be very inconvenient if you need to just temporarily insert a non-preexiting element. For example:

###### *snippet 4*
```cpp
	{
		std::array<std::string, 3> strings1{ "elephant", "hippopotamus", "rhinoceros" };

		struct CItem {
			CItem(std::string* string_ptr) : m_string_ptr(string_ptr) {}
			std::string* m_string_ptr = nullptr;
		};
		struct CF {
			static double avg_word_length(const std::list<CItem>& container) {
				int cummulative_length = 0;
				int num_words = 0;
				for (const auto& item : container) {
					if (item.m_string_ptr) {
						cummulative_length += item.m_string_ptr->length();
						num_words += 1;
					}
				}
				double retval = double(cummulative_length);
				if (1 <= num_words) {
					retval /= double(num_words);
				}
				return retval;
			}
		};

		std::list<CItem> container1;
		for (auto& str : strings1) {
			container1.push_back(CItem(&str));
		}

		std::cout << "avg1: " << CF::avg_word_length(container1) << std::endl;
		{
			// logically, but not structurally safe
			std::string l_string("giraffe");
			container1.push_back(CItem(&l_string));
			std::cout << "avg2: " << CF::avg_word_length(container1) << std::endl;
			container1.pop_back();
		}
		{
			std::string l_string("gnu");
			container1.push_back(CItem(&l_string));
			std::cout << "avg3: " << CF::avg_word_length(container1) << std::endl;
			container1.pop_back();
		}

	}
```

In this example, the eventual completed lifetime checker will presumably not permit those last two `push_back()`s. Again, one solution would be to temporarily move the list of references into the local scope before inserting the reference to the local variable and then move it back after the reference has been removed. That would be fairly inexpensive as moving a list is pretty cheap, but again, you could imagine using other container types that aren't so cheap to move. Another solution would be to use `weak_ptr`s in place of raw pointers and have all the strings owned by `shared_ptr`s. That would work, but it's kind of intrusive and expensive. 

Certainly neither of these cases are deal-breakers that would rule out strict adherence to the lifetime checker. The point is more that problematic cases like these could be addressed more cleanly and efficiently if the lifetime checker were augmented by a more comprehensive set of (familiar) logically safe elements. And that perhaps this fact is being overlooked due to a strong preference for "zero-overhead" elements over those that may incur some run-time overhead. These examples perhaps demonstrate that in some cases, impoverishing the set of available logically safe elements as a means of encouraging the use of "zero-overhead" elements can be a false economy. 

Specifically, in the two examples given, the missing elements that could be helpful might be vectors (and dynamic containers more generally) that support logically safe references to their elements, and general logically safe non-owning pointer/references that can point to (stack allocated) local variables as well as dynamic (heap allocated) objects. If the gsl, for example, is not going to provide these sorts of elements, then the checker's mechanism for supporting third party "safe" elements needs to be flexible enough to accomodate them.

