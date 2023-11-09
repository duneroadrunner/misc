Oct 2023

## Memory safe C++ vs Rust language design limitations


This is not about language syntax or ecosystem. This is about the functionality and performance limitations inherent to the language design.

First, at the time of writing it may still need to be clarified that an enforced memory-safe subset of C++ roughly analogous to Rust's does indeed exist, and that there is essentially an [existence proof](https://github.com/duneroadrunner/scpptool) of it.

Some have suggested that Rust's efficient memory safety depends on its "exclusivity of mutable references" policy, and could not be achieved without it. But this is not really the case. Consider that, for example, `RefCell`s allow you to in some ways evade the "exclusivity of mutable references" restrictions without compromising Rust's memory safety. In fact, for memory safety, Rust relies on only a select subset of those restrictions (along with the "borrowing"/"scope lifetime" restrictions). Namely, Rust's memory safety relies on the exclusivity between associated mutable "borrowing" and "lending" references (of which `Cell`s and `RefCell`s are not exempt).

But even this select subset is actually more than what's strictly necessary for (efficient) memory safety. So the memory-safe subset of C++ alluded to enforces an even more select "exclusion of mutation" (along with "scope lifetime" restrictions similar to Rust's), in order to better accommodate pre-existing C++ code. Namely the exclusivity between references to the contents of a "dynamic" container (or the target of a "dynamic" owning pointer) and the ability to modify the "structure"/location/existence of the contents. 

The following is an example of an unsafe reference to an element of a (legacy) standard C++ vector and a (safe) reference to an element of a vector in our memory-safe subset of C++:

```cpp
    #include <iostream>
    #include <vector>
    #include "msemsevector.h"
    
    void main() {
        MSE_SUPPRESS_CHECK_IN_XSCOPE { // just indicating a region of unsafe code
            auto stdvec = std::vector<int>{ 1, 2 };
            int* int_ptr1 = &(stdvec.at(0));
            stdvec.clear();
            // not good
            std::cout << *int_ptr1 << ' ';
        }

        {
            auto xsvec = mse::rsv::xslta_vector<int>{ 1, 2 };
            
            //int* int_ptr2 = &(xsvec.at(0));
            /* The line above wouldn't compile because xsvec.at(0) does not return 
            a reference to an int, but rather a "proxy reference" object that 
            implicitly converts to an int. */
            
            {
                auto bf_vec = mse::rsv::make_xslta_borrowing_fixed_vector(&xsvec);
                /* The line above creates a fixed-sized vector (interface) that "borrows" 
                (exclusive access to) the contents of xsvec. */

                int* int_ptr2 = &(bf_vec.at(0));

                //bf_vec.clear(); // wouldn't compile because bf_vec doesn't have a `clear()` member function
                //xsvec.clear(); // this either would have no effect or throw an exception because `xsvec`s contents are being "borrowed"

                std::cout << *int_ptr2 << ' ';

                // the borrowed contents of xsvec are returned here when bf_vec is destructed
            }
            std::cout << int(xsvec.at(0)) << ' ';
        }
    }
```

In this example we see that the memory-safe version incurs some theoretical extra overhead by instantiating a "borrowing fixed vector". Some theoretical overhead would also be incurred on any operation that could resize or relocate the contents of the vector. Safe Rust does not incur such overhead. But instead, Rust incurs theoretical overhead when assigning the value of one element in a container (like a vector) to another element of the same container, by effectively either requiring the instantiation of slices or an intermediate copy. The Rust overhead being more likely to occur in performance-critical inner loops than the memory-safe C++ overhead.

Of course, in many or most cases, modern compiler optimizers can eliminate the overhead in both languages.

Some may argue that's Rust's "exclusivity of mutable references" policy is more intuitively elegant. Maybe, but it isn't any more valid or effective at enforcing memory safety. Rust's "exclusivity of mutable references" policy has benefits apart from memory-safety (i.e. reliability and the prevention of (low-level) aliasing mistakes). But those benefits are a trade-off with flexibility. (For example, Rust's restrictions on cyclic references.) My take is that the Rust language design is fairly optimal given the trade-offs it adopted. But I don't see those trade-offs being obviously preferable overall to others that could be chosen. And in particular, the ones chosen for our memory-safe subset of C++.

Just as Rust's `RefCell`s allow you to substitute the "draconian" compile-time "exclusivity of mutable references" enforcement with more flexible run-time enforcement, our safe subset of C++ allows you to substitute the compile-time enforced lifetime restrictions (on references) with more flexible run-time enforcement. Indeed, essentially unconstrained references are possible (and available) in our safe subset. This enables, for example, cyclic references (in the safe subset and without needing the targets to be "pinned" or any such thing). And it also means pointers in legacy C/C++ code can be directly replaced with an essentially compatible (safe) run-time checked counterpart.

These important capabilities can't really be (reasonably) duplicated in Rust due to the fact that Rust does not support anything like move constructors and, unlike C++, does not consider a moved object to have left behind a remnant that needs to be "dropped"/destructed.

Another difference is the way (raw) pointers are dealt with. In our safe C++ subset, raw pointers, like (raw) references, are ensured to be always valid (i.e. never dangling and never null) and so the dereferencing of pointers is part of the safe subset. Rust, on the other hand, has no protection against dangling pointers, and so the dereferencing of pointers is not part of its safe subset. But comparison of pointers *is* part of Safe Rust, so Safe Rust supports comparison with possibly dangling pointers.

This is arguably a little disconcerting. Does the result of comparison with a dangling pointer have any intrinsic meaning? Does the repeated comparison of unmodified possibly dangling pointers reliably produce the same results? My understanding is that the answer is determined by the current implementation of the underlying llvm compiler used by the Rust compiler. One could imagine this, at least theoretically, limiting opportunities for possible future optimizations.

