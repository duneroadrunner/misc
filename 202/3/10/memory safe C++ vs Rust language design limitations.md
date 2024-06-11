Dec 2023 (updated June 2024)

## Memory safe C++ vs Rust language design limitations


This is not about language syntax or ecosystem. This is about the functionality and performance limitations inherent to the language design.

First, at the time of writing it may still need to be clarified that an enforced memory-safe subset of C++ roughly analogous to Rust's does indeed exist, and that there is essentially an [existence proof](https://github.com/duneroadrunner/scpptool) of it.

*TLDR:
There seems to be a common misconception that Safe Rust's universal "exclusivity of mutable references" restriction is required for efficient memory safety. It is not. In fact it has relatively minor (positive) effects on code correctness, (negative effects on) performance, and (negative effects on) ergonomics.
A bigger issue is Rust's lack of move constructors. It is significantly crippling. For example, it hinders implementation of self/mutable/cyclic references in safe Rust.
Also, Safe Rust supports comparison (and copying) of (potentially) dangling pointers. That's a little disconcerning.*

Some have suggested that Rust's efficient memory safety depends on its "exclusivity of mutable references" policy, and could not be achieved without it. But this is not really the case. Consider that, for example, `RefCell`s allow you to in some ways evade the "exclusivity of mutable references" restrictions without compromising Rust's memory safety. In fact, for memory safety, Rust relies on only a select subset of those restrictions (along with the "borrowing"/"scope lifetime" restrictions). Namely, Rust's memory safety relies on the exclusivity between associated "borrowing" and mutable "lending" references (of which `Cell`s and `RefCell`s are not exempt).

But even this select subset is actually more than what's strictly necessary for (efficient) memory safety. So the memory-safe subset of C++ alluded to enforces an even more select "exclusion of mutation" (along with "scope lifetime" restrictions similar to Rust's), in order to better accommodate pre-existing C++ code. Namely the exclusivity between references to the contents of a "dynamic" container (or the target of a "dynamic" owning pointer) and the ability to modify the "structure"/location/existence of the contents. 

Thematically, you might think of it vaguely as the memory-safe subset of C++ embracing the "don't pay for what you don't use (but often pay full price for what you do use)" principle in terms of accepting restrictions in return for memory safety, whereas Rust takes more of a "pay the monthly membership dues for reduced or no-cost benefits whenever you need them" approach. But with the (safe) Rust approach, sometimes the desired item is not available at any reasonable price. (Eg. self/mutual/cyclic references).

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
                //xsvec.clear(); // this either would have no effect or throw an exception because `xsvec`'s contents are being "borrowed"

                std::cout << *int_ptr2 << ' ';

                // the borrowed contents of xsvec are returned here when bf_vec is destructed
            }
            std::cout << int(xsvec.at(0)) << ' ';
        }
    }
```

In this example we see that the memory-safe version incurs some theoretical extra overhead by instantiating a "borrowing fixed vector". Some theoretical overhead would also be incurred on any operation that could resize or relocate the contents of the vector. Safe Rust does not incur such overhead. But instead, Rust incurs theoretical overhead, for example, when you need to hold multiple (including `mut`) references to different elements of a container. For example, assigning the value of one element in a container (like a vector) to another element of the same container, effectively requires the instantiation of slices or an intermediate copy. The Rust overhead being more likely to occur in performance-critical inner loops than the memory-safe C++ overhead.

(Though one might imagine the calculus changing if we ever get hardware with an accelerated copy instruction that is only reliable when the source and destination don't alias/overlap.) Of course, in many or most cases, modern compiler optimizers can eliminate the overhead in both languages.

The additional run-time overhead in both languages is often in the form of run-time checking, which usually comes with potential "unreliability" (in the case that the check fails) that is not eliminated at compile-time. As we noted, each language distributes its additional run-time overhead, and so too its "uneliminated" unreliability, in different places. Here too there may be an argument that can be made that when a run-time safety check fails, with the safe subset of C++, it's more likely to correspond to an actual bug (i.e. behavior that the programmer did not intend). For example, when the programmer needs to hold multiple (possibly `mut`) references to elements of a (contiguous) container, instead of just ensuring that the references don't alias (i.e. making each element a `RefCell<>`), to reduce overall run-time overhead, the programmer may resort to splitting the container into slices. While not questioning the necessity of this design, this requires the programmer to provide a split index, which introduces an "artificial additional" potential failure point. That is, the programmer could, for example, unintentionally provide an out-of-bounds split index resulting in a run-time panic, even if the intrinsic requirements of the operation (i.e. that the specified indices of the objects to be referenced are valid and distinct) were properly met.

Perhaps more concerning is an [anecdote](https://loglog.games/blog/leaving-rust-gamedev/) about what happens with safe Rust at scale. Attempting to paraphrase here: The author suggests the inevitability of having large "state" objects (ultimately global state objects in the author's case) and having functions that end up holding references to many different (struct) fields in those objects. The author suggests that eventually, as the number of used fields grows, passing (references to) each field as a separate parameter to the functions becomes too unergonomic, and they end up resorting to just passing `RefCell<>` references to the object.

Now quoting directly from the article: "I'm mentioning this in a section on global state, because the existence of such limitations becomes especially apparent once things are made global, because it becomes very easy to accidentally touch a RefCell<T> that another part of the codebase is touching through some global reference. Again, Rust developers will say this is good, you're preventing a potential bug!, but I'll again defer to saying that I don't think I've felt many cases where this has actually saved me from doing something wrong, or where doing this in a language without such restrictions would cause an issue."

(But one might wonder if this "mass of parameters" ergonomic hurdle could be somewhat alleviated with help from a more proactive IDE, or perhaps in a manner vaguely analogous to the way Rust's `?` (question mark) operator alleviated the tedious boiler plate code involved with error handling.)

Also, for example, in cases where you execute many iterations (of a loop say) in which you're holding multiple (including `mut`) references to elements in a container, but some of those references remain targeted at the same element over multiple iterations, in (safe) Rust you may end up effectively having to release and reacquire those references on every iteration, where each reacquisition may incur run-time overhead. Whereas with (memory-safe) C++, the references (pointers) can be stored/preserved across iterations incurring reacquisition overhead only when the references are changed. (There is a notable related [anecdote](https://ceronman.com/2021/07/22/my-experience-crafting-an-interpreter-with-rust/) where the author ended up resorting to unsafe Rust to avoid non-trivial overhead due to reacquisition of references in the implementation of an interpreter.)

And of course, in the more general case where you have a collection of entities that reference each other dynamically in somewhat arbitrary ways, and/or with varying lifetimes, safe Rust essentially forces the programmer to emulate, with significant overhead, a more flexible memory management system. Whereas with (memory-safe) C++, these situations can be handled more directly with significantly less overhead. This is, for example, an issue in many types of real-time video games and simulations. (There was a notable case where a game developer [expressed](https://youtu.be/4t1K66dMhWk) this observation. And it's perhaps also notable that, for example, checking one of the most popular Rust libraries for handling this kind of situation, we see that it makes liberal use of [unsafe code](https://github.com/bevyengine/bevy/blob/41db723c5cacee53b3d8c5ad831a9c0d93ec2652/crates/bevy_ecs/src/world/unsafe_world_cell.rs#L141-L145).)

Some may argue that's Rust's "exclusivity of mutable references" policy is more intuitively elegant. Maybe, but it isn't any more valid or effective at enforcing memory safety. Rust's "exclusivity of mutable references" policy has benefits apart from memory-safety (i.e. the prevention of (low-level) aliasing mistakes). But those benefits are a trade-off with flexibility. (For example, Rust's restrictions on cyclic references.) My take is that the Rust language design is fairly optimal given the trade-offs it adopted. But I don't see those trade-offs being obviously preferable overall to others that could be chosen. And in particular, the ones chosen for our memory-safe subset of C++.

Just as Rust's `RefCell`s allow you to substitute the "draconian" compile-time "exclusivity of mutable references" enforcement with more flexible run-time enforcement, our safe subset of C++ allows you to substitute the compile-time enforced lifetime restrictions (on references) with more flexible run-time enforcement. Indeed, essentially unconstrained references are possible (and available) in our safe subset. This enables, for example, cyclic references (in the safe subset and without needing the targets to be "pinned" or any such thing). And it also means pointers in legacy C/C++ code can be directly replaced with an essentially compatible (safe) run-time checked counterpart.

These important capabilities can't really be (reasonably) duplicated in Rust due to the fact that Rust does not support anything like move constructors and, unlike C++, does not consider a moved object to have left behind a remnant (that will be "dropped"/destructed).

Another difference is the way (raw) pointers are dealt with. In our safe C++ subset, raw pointers, like (raw) references, are ensured to be always valid (i.e. never dangling and never null) and so the dereferencing of pointers is part of the safe subset. Rust, on the other hand, has no protection against dangling pointers, and so the dereferencing of pointers is not part of its safe subset. But comparison of pointers *is* part of Safe Rust, so Safe Rust supports comparison with possibly dangling pointers.

This is arguably a little disconcerting. Does the result of comparison with a dangling pointer have any intrinsic meaning? Does the repeated comparison of unmodified possibly dangling pointers reliably produce the same results? My understanding is that the answer is determined by the current implementation of the underlying llvm compiler used by the Rust compiler. One could imagine this, at least theoretically, limiting opportunities for possible future optimizations.

