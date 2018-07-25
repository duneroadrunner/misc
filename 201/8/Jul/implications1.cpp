
/* By default, elements in the SaferCPlusPlus library don't trust raw pointers to be safe and do not support them
in certain interfaces, instead requiring (safe) "scope" pointers. However, the following directive will cause the
library to alias (safe) scope pointers to their corresponding raw pointers. */
#define MSE_SCOPEPOINTER_DISABLED

#include "msemstdvector.h"
#include "msescope.h"
#include "mseregistered.h"

#include <string>
#include <vector>
#include <list>

int main(int argc, char* argv[]) {

	{
		/* This code block is a version of the code example ("snippet 3") here:
		https://github.com/duneroadrunner/misc/blob/master/201/8/Jul/implications%20of%20the%20lifetime%20checker%20restrictions.md#snippet-3
		implemented using elements from the SaferCPlusPlus library to ensure the safety of the references in question while
		avoiding the potentially expensive moves that would otherwise be required.
		*/

		struct CState {
			typedef std::array<std::string, 10> type1;
			type1 m_data;
			int m_count = 0;
		};
		struct CSequence : public mse::mstd::vector<CState> {
			typedef mse::mstd::vector<CState> base_class;
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

		{
			/* Just a pointer to v1's base_class, which is a vector. */
			CSequence::base_class* const v1_xsptr = &v1;

			/* Instantiating a "size_change_lock" object "locks" the specified container (vector in this case), for the
			lifetime of the lock object, so that any container operation that could potentially invalidate any contained
			element will instead throw an exception. */
			auto v1sc_lock1 = mse::mstd::make_xscope_vector_size_change_lock_guard(v1_xsptr);

			/* From the "lock" object we can obtain a direct pointer to any of the container's elements. The pointer is
			ensured to be valid until the end of the scope. */
			auto v1_0_xsptr = v1sc_lock1.xscope_ptr_to_element(0);

			v2.reset_sequence(*v1_0_xsptr);

			/* In this case the lifetime checker, when completed, would be able to verify the safety without any run-time
			overhead whatsoever, making the use of a "lockable" vector unnecessary. But in a case like the next one, the
			lifetime checker will not be as helpful.  */
		}

		mse::mstd::vector<CSequence> vectors = mse::mstd::vector<CSequence>{ v1, v2 };
		{
			/* So here we're first locking "vectors" (the vector of "CSequence"s).  */
			auto vectors_sc_lock1 = mse::mstd::make_xscope_vector_size_change_lock_guard(&vectors);

			/* Then obtaining a pointer to the first CSequence. */
			auto vectors_0_xsptr = vectors_sc_lock1.xscope_ptr_to_element(0);

			/* Here we're just converting the pointer to the CSequence to a pointer to its base class, which is a vector.  */
			CSequence::base_class* const v1_xsptr = vectors_0_xsptr;

			/* And here we're locking the the CSequence vector. */
			auto v1_sc_lock1 = mse::mstd::make_xscope_vector_size_change_lock_guard(v1_xsptr);

			/* And again obtaining a (zero-overhead) direct pointer to the first element. And that pointer is guaranteed
			to be valid until the end of the scope. */
			auto v1_0_xsptr = v1_sc_lock1.xscope_ptr_to_element(0);

			vectors.at(1).reset_sequence(*v1_0_xsptr);

			/* The lifetime checker by itself would not be able to determine whether or not the pointer/reference argument
			in the previous function call would remain valid for the duration of the function call. By "structure locking",
			at run-time, the containers that hold the target of the pointer, we can be assured that no invalid access will
			be made through the pointer. This allows us to avoid having to move the target out of its container before the
			call and back into the container after the call. */
		}
	}

	{
		/* This code block is a version of the code example ("snippet 4") here:
		https://github.com/duneroadrunner/misc/blob/master/201/8/Jul/implications%20of%20the%20lifetime%20checker%20restrictions.md#snippet-4
		implemented using elements from the SaferCPlusPlus library to ensure the safety of the references in question.
		*/

		typedef std::string* const string_xsptr_t;

		/* mse::TRegisteredPointer<> is a safe non-owning smart pointer that doesn't impose any restrictions on the
		manner in which its targets are allocated or deallocated. In particular, it can point to (stack allocated) local
		variables declared in any scope. */
		typedef mse::TRegisteredConstPointer<string_xsptr_t> string_xsptr_regptr_t;

		std::array<std::string, 3> strings1 { "elephant", "hippopotamus", "rhinoceros" };

		/* Wrapping a type in the mse::TRegisteredObj<> transparent template wrapper allows it to be targeted by
		registered pointers.
		So the following is an array of pointers to strings contained in another array. Each of these pointers can
		(and will) themselves be targeted by registered pointers. */
		std::array<mse::TRegisteredObj<string_xsptr_t>, 3> strings1_xsptrs{ &(strings1.at(0)), &(strings1.at(1)), &(strings1.at(2)) };

		struct CItem {
			/* Instead of a pointer to a string, each item is going to hold a registered pointer to a pointer to a
			string. */

			CItem(string_xsptr_regptr_t string_xsptr_regptr) : m_string_xsptr_regptr(string_xsptr_regptr) {}
			string_xsptr_regptr_t m_string_xsptr_regptr;
		};
		struct CF {
			static double avg_word_length(const std::list<CItem>& container) {
				int cummulative_length = 0;
				int num_words = 0;
				for (const auto& item : container) {
					if (item.m_string_xsptr_regptr) {
						cummulative_length += (*(item.m_string_xsptr_regptr))->length();
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
		for (const auto& string_xsptr : strings1_xsptrs) {
			string_xsptr_regptr_t string_xsptr_regptr = &string_xsptr;

			container1.push_back(CItem(string_xsptr_regptr));
		}

		std::cout << "avg1: " << CF::avg_word_length(container1) << std::endl;

		{
			// logically, but not structurally safe

			/* The target object remains a (stack allocated) local variable. */
			std::string l_string("giraffe");

			/* This is a pointer to the string. */
			mse::TRegisteredObj<string_xsptr_t> string_xsptr_reg = &l_string;

			/* This is a registered pointer to the pointer to the string. */
			auto string_xsptr_regptr = &string_xsptr_reg;

			container1.push_back(CItem(string_xsptr_regptr));
			std::cout << "avg2: " << CF::avg_word_length(container1) << std::endl;
			container1.pop_back();

			/* Even if we had forgotten to do the popback() call, it would still be safe. After the end of the scope
			the pointer to the string referenced by the last container element would no longer be valid, but registered
			pointers know when their target becomes no longer valid and will throw an exception if you try to access
			the no longer valid target. */

			/* Registered pointers do have run-time overhead, but if you can use them to avoid extra heap allocations,
			it's generally well worth it. */
		}

		{
			// logically, but not structurally safe
			std::string l_string("gnu");
			mse::TRegisteredObj<string_xsptr_t> string_xsptr_reg = &l_string;
			container1.push_back(CItem(&string_xsptr_reg));
			std::cout << "avg3: " << CF::avg_word_length(container1) << std::endl;
			container1.pop_back();
		}

		/* In this solution we added an extra level of indirection to the references stored in the list. We didn't
		necessarily need to do that. We could have targeted the registered pointers directly at the strings. But
		that would have required wrapping the string type in the mse::TRegisteredObj<> transparent template wrapper,
		which is slightly "intrusive". */
	}

	return 0;
}
