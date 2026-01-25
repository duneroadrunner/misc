Jan 2026

# Benign-by-luck buffer overflow in wget encountered during PoC auto-translation to memory-safe subset of C++

This article discusses the (proof-of-concept) [auto-translation of wget](https://github.com/duneroadrunner/SaferCPlusPlus-AutoTranslation2/tree/master/examples/wget-1.25.0) to a memory-safe subset of C++. The intention of this undertaking was not primarily to find buffer overflow bugs in wget. As indicated, the one we found ends up being essentially benign. 

Neither was the primary intention to produce a practical, expedient solution for addressing memory safety in legacy C/C++ code bases. Though demonstrating such a solution might be considered a secondary goal.

Neither was the primary intention to draw attention to the existence of this practical (high-performance) statically-enforced memory-safe [subset](https://github.com/duneroadrunner/scpptool/blob/master/approach_to_lifetime_safety_summary.md) of C++. (A phenomenon of which many seem to remain unaware or skeptical.)

The primary motivation for this effort was to showcase the fact that the subset's memory safety is not achieved at the expense of a dramatic reduction in practical expressive power. In other words, (reasonable) things that you can express in traditional (unsafe) C++, can generally be just as easily expressed in the safe subset. (Sometimes with modest additional run-time overhead.) We try to show this by demonstrating the auto-translation of existing real-world applications to the safe subset using a direct one-for-one mapping of each of the code elements in the original program to a corresponding element in the safe subset. I.e. the safe subset is powerful enough to support a corresponding element (with the same behavior) for each (reasonable) element in the traditional (unsafe) language. (An example of an "unreasonable" element would be a type-punning cast to an unrelated type.)

Now, as many know, all Turing-complete (memory-safe) languages (i.e. almost all of them) have exactly the same theoretical expressive power. In practice though, we'd want to consider the "reasonableness" of the expressions in terms of ergonomic and performance costs.

### The buffer overflow

So, with that out of the way, let's get to the buffer overflow (in version 1.25.0 of wget):

in `src/init.h`:

	#define MAX_LONGOPTION 26

in `src/main.c`:

	struct cmdline_option {
	  char long_name[MAX_LONGOPTION];
	  char short_name;
	  enum {
		OPT_VALUE,
		OPT_BOOLEAN,
		OPT_FUNCALL,
		/* Non-standard options that have to be handled specially in
		   main().  */
		OPT__APPEND_OUTPUT,
		OPT__CLOBBER,
		OPT__DONT_REMOVE_LISTING,
		OPT__EXECUTE,
		OPT__NO,
		OPT__PARENT
	  } type;
	  const void *data;             /* for standard options */
	  int argtype;                  /* for non-standard options */
	};

later in `src/main.c`:

	static struct cmdline_option option_data[] =
	  {
		{ "accept", 'A', OPT_VALUE, "accept", -1 },
		{ "accept-regex", 0, OPT_VALUE, "acceptregex", -1 },

...

		{ "ftp-user", 0, OPT_VALUE, "ftpuser", -1 },
		IF_SSL ( "ftps-clear-data-connection", 0, OPT_BOOLEAN, "ftpscleardataconnection", -1 )

So if ssl is enabled, then the `long_name` (`char` array) field will be initialized with "ftps-clear-data-connection". That string is 26 characters long. 27 including the null terminator. But the `char` array only has room for 26 `char`s, so (when compiled as C) the `char` array will just end up without a null terminator. 

But in more than one place later in the program, the `long_name` field is used as a null-terminated string. This sort of situation, of course, is potentially dangerous in general. 

But in this particular case, the very next field in the struct, `short_name`, is a `char` that happens to be set to a value of `0`. (Because this particular command line option happens to not have an associated "short name".) So, presumably somewhat by luck, this field acts as a "backup" null terminator for the preceding string buffer field. 

But when the code is compiled as C++, you instead get a compile error because, unlike C, C++ requires that a `char` array be large enough to accommodate the null terminator of the initialization string. 

So even before translation to the memory-safe subset, simply (auto-converting to and) compiling as C++ catches this bug. (Just to be clear, this specific bug is not in a particularly "sensitive" part of the program and arguably would likely have had limited exploitability in any case, but one could imagine the same kind of bug being more consequential in other circumstances.)

### Auto-translation of legacy code

If you've ever tried it, you may have also had the experience of realizing how much you underestimated the sheer number of, and types of, compile errors produced when trying to compile a legacy C code base like wget as C++. Conveniently, scpptool has feature that helps auto-convert from C to a subset of C that is also a subset of C++. (Or to be more accurate, a subset of "C/C++" that will compile under clang++, with or without a boatload of warnings about how such and such code is technically a C99 extension rather than ISO standard C++, etc..) 

While compiling as C++ may catch a few bugs, by itself, of course, it doesn't help much. So once you get your code to compile as C++, you can then use scpptool to auto-translate the code to the memory-safe subset of C++ it enforces. Note that the auto-translation does not produce "idiomatic" or performance-optimal code. Instead, as we mentioned, it modifies the code by generally doing a one-for-one substitution of each potentially unsafe C++ element with a corresponding safe element from the SaferCPlusPlus library that strives to have the same behavior (for valid behaviors).

While this kind of auto-translated code will incur extra run-time overhead, there are also benefits. First, being a one-for-one translation, anyone who understands the original code should almost equally understand the translated code. All variable and field names, and comments (and even whitespace formatting) are preserved in the auto-conversion.

The one-for-one nature of the translation should also ensure limited opportunities for the introduction of new bugs. (In contrast to an "idiomatic" rewrite.) All the original algorithms, loops, and branch conditions should remain the same.

### Unique suitability of the C++ safe subset as a migration target due to its (retained) expressive power

Ok, so we've demonstrated this auto-translation of a real-world nontrivial program. But could one not do similar one-for-one translations to other memory-safe languages?

Well, of course it depends on what we mean by "similar". But I'll point out that it may be more challenging, for example, for (the current crop of) "affine/linear"/"necessarily-trivial-destructive-move" memory-safe "system" languages (including Rust). 

For example, C/C++ native pointers are essentially unrestricted as to which object they can point to, as long it's of the appropriate type. In particular, they are unrestricted with respect to the lifetime of the object they can point to. The use of these pointers can't, in the general case, be verified as safe at compile-time while retaining their traditional flexibility. The claim is that such a feat requires run-time safety mechanisms. 

For example, the [Fil-C](https://fil-c.org/) solution uses a universal garbage collection run-time mechanism to ensure memory safety without imposing constraints on the (valid) use of pointers. In particular, as I understand it, local variables are allocated on the GC heap rather than the traditional stack.

Unlike (the often more pragmatic) Fil-C, scpptool auto-translated code does not (and has no ability to) change the way that stack allocation works. Instead pointers to local (stack-allocated) objects are, by default, replaced with non-owning smart pointers that cooperate with their target objects to make sure that they are "informed" when and if their target is deallocated while still the target of the smart pointer.

scpptool's associated SaferCPlusPlus library provides a selection of non-owning smart pointers. The ones used by the auto-translator by default to substitute for native pointers permit the deallocation of their target object while they are still targeting them (just as native pointers do). I.e. these particular smart pointers are, in some sense, allowed to become "dangling". But safely so, due to fact that the smart pointers know when they become "dangling", because they are informed (by the object's destructor) when the object's destruction occurs. 

The implementation of these non-owning smart pointers rely on a couple of language properties. First is that, if defined, an object's destructor is always called when the object's lifetime ends at its original location (during the program's execution). And the second is support for "static" inheritance (i.e. the aspects of inheritance unrelated to virtual methods), and the ability to use an object as its base class type while also being able to have data members apart from those of its base class. 

My understanding is that the current crop of necessarily-trivial-destructive-move system languages do not support either of these properties. I don't know if there are practical equivalent workarounds for these properties, but none that I'm aware of (for some definition of "practical"). 

### Idiomatic performance

Now, it's probably not appropriate for the default pointer/reference in a "system" language to have this kind of run-time overhead. And indeed, unlike this auto-translated code, idiomatic pointers/references of the scpptool-enforced safe subset are "zero-overhead". They are basically just statically restricted versions of the traditional native pointers/references. 

And for performance-sensitive parts of auto-translated code (of which wget presumably has none?), it is more-or-less [straightforward](https://github.com/duneroadrunner/SaferCPlusPlus-AutoTranslation2/tree/master/examples/lodepng/lodepng_partial_reoptimization1) to manually replace these runtime-checked pointers with the idiomatic statically-checked zero-overhead ones, where suitable. (The challenge is determining where the scpptool static analyzer/enforcer will and won't consider zero-overhead pointers "suitable".) But of course the ["90/10" principle](https://en.wikipedia.org/wiki/Program_optimization#Bottlenecks) suggests that, even in performance-sensitive applications, most of the code is not actually performance-sensitive.

To put it plainly, as far as I know, there is no memory-safe language available with better overall (theoretical at least) run-time performance and efficiency than the scpptool-enforced subset of C++. (Though one might argue that others are essentially tied.)

### Expressive power, not just for dealing with legacy code

I think it was Benjamin Franklin who once proclaimed "Those who would give up essential liberty (to use references whose lifetime safety can't be verified statically) to purchase a little temporal (memory) safety, deserve neither liberty nor safety." :)

While zero-overhead references are the preferred default, the fact is that for a small but not insignificant proportion of use cases, statically-verified zero-overhead references are just too inflexible. 

Such cases would include the auto-translation task we're demonstrating here, but also basically any case where the target of a reference (data member) cannot be statically proven to outlive the reference itself. Which would include, for example, cyclic or mutual references. Which would include things like tree structures where child nodes hold "back references", entity component systems and (famously) data structures like doubly-linked lists.

In the scpptool-enforced subset, there is a straightforward option for these situations. You can just use a non-owning run-time-checked pointer that does not have the same constraints on the lifetime of its target object.

In languages that can't support these kinds of non-owning run-time-checked pointers, the situation may be more difficult. Often, the only safe options involve imposing potentially intrusive and costly requirements on how the target object is allocated. For example, changing the target object to be owned and allocated by an owning pointer, or to be an (owned) element in a vector or some other container. The alternative being to resort to unsafe code.

In many, if not most cases, these solutions with intrusive and costly allocation restrictions can be accommodated just fine. But in other cases, they are more problematic and I think at this point there is significant empirical evidence that programmers often resort to using unsafe code in these situations.

The prevailing argument has been that there is no viable alternative, so the limitations just have to be accepted. Given the not-yet-finished state of the scpptool solution, one could argue that this position still has some validity. But for those who've been following, the scpptool solution has arguably already demonstrated the viability of an alternative with a comparatively appealing set of limitations and tradeoffs (in terms of "expressivity"). 

With limited resources, there may not be much danger of the scpptool project itself becoming directly relevant to most anytime soon. But hopefully it can at least help raise the floor for what is considered possible for C++. And demonstrate that in certain aspects (and in contrast to prevailing wisdom), that floor may be higher than what has been, or can be, achieved with other language designs.
