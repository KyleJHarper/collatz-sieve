# Harper's Sieve of Collatz

__A Monotonic Binary-Tree Reduction of the Collatz Search Space__


# TODO
* Cost model stuff with data types, extending support
* Save and load
* 128-bit compiler support is NOT optional



# Overview

An implementation of the data structure and sieve outlined in Kyle Harper's analysis and optimization of the
[Collatz Conjecture's](https://en.wikipedia.org/wiki/Collatz_conjecture) problem space.

The code builds a binary tree in a unique, deterministic manner which allows the classification of parents and children into
__F-G Chains__.  These chains create _coverage_ via __High-Water Mark__ nodes.  The result eliminates large subtrees, leaving less
than 1% of the search space ℕ after 33 levels.  Such a tree can be built in a few seconds on a desktop CPU, requiring only ~40MB of
memory.  Larger trees are possible, using more resources, and growing closer to 100% coverage (though never 100%).  The following
table shows coverage using `uint64_t` on a desktop PC (i5-14600, DDR5).

| Tree Levels | Coverage | Time (sec) |
| ----------: | -------: | ---------: |
|          10 |   92.58% |         <1 |
|          20 |   97.14% |         <1 |
|          30 |   98.81% |         <1 |
|          35 |   99.12% |          2 |
|          40 |   99.34% |         50 |

This implementation is written in C++ and supports fixed-widths up to 128 bits, (if your compiler has it), as well as arbitrary
precision via [GMP](https://gmplib.org/).  It assumes, to an extent, you're on a 64-bit platform, but might run on 32-bit.

Starting with version 3.x, we attempt to follow [SemVer](https://semver.org/).

### An Aside from Kyle

This was an R&D project, focused on education and delving into insights I garnered after many, many hours with a pen and paper. The
choice of language and building a stable API happened as a natural consequence during development.  Take it for what it is and
don't overthink it. If you hate C++, port it.  If you hate _my_ C++, fork it.  If you need a feature, make a pull request.

I __highly__ recommend you read [the paper (PENDING)]() on this before you start playing with the code.


# Present State

This project is semi-active.  No new features are planned, but bugs will be fixed, if any.  Merge requests are welcome, especially
in the following areas:

* Bug fixes for legtimate correctness issues.
* Non-breaking optimizations.
* Build and portability updates, especially for non-linux.
* Improvements to test tooling.

# API and Programs

### Quick Start

Literally this:

```
#include "collatz/binary_tree.hpp"

BinaryTree<uint64_t> tree(3);
```

### Limits

The API accepts any native, fixed-width type up to 128 bits, such as `uint8_t`, `uint16_t`, etc.  GCC/Clang's 128-bit type has been
typedef'd to `uint128_t` for convenience.  The API expects an unsigned type.

The API supports >128 bits with GMP's `mpz_class`.  It has been highly optimized, but is still 2-5x slower than native types for
the same bit size.

### Classes & Facades

`BinaryTree` A facade which builds a tree of type `BinaryTreeMaterializedImpl` or `BinaryTreeImplicitImpl`, removing nodes and
subtrees meeting High-Water Mark.  You may build an implicit or materialized tree directly, but you probably shouldn't.  Once
built, the uncovered positions are provided in a vector of `Node` objects (Materialized) or a `NodeBitmap` bitmap (Implicit). As of
version 4.0.0, the Implicit tree is the default, and you should use it.

`Collatz` A class which can build a sequence and give you warm-fuzzy OOP feels, but its real value is in the static members for
efficiently processing steps, finding metadata, and so forth.

`BinaryTreeMath` A static-only class which contains the math supporting the data structure and the algorithms.

`NodeBitmap` A facade in front of the `FlatHashBitmapImpl` (only implementation thus far) which extends a
[CRoaring Bitmap](https://github.com/RoaringBitmap/CRoaring) to support >64 bits.  This class is extremely useful for compact
representation of on/off or true/false flags for node positions in a tree.

Other helpful tools exist in namespaces, such as `CollatzConstants`, `Exponents`, `AffineStride`, etc.

### Programs

Several programs are emitted (or are written in Python).

| Program               | Status     | Description |
| :-------------------- | :--------- | :---------- |
| `affine_shortcut.py`  | Working    | Empirically tests the worst-case scenario of our affine shortcut technique. |
| `binary_stepper.py`   | Working    | Emits the binary representation of N-args (numbers) for comparison. |
| `collatz_compression` | Working    | Demonstrates some of the optimizations when testing sequences. |
| `coverage`            | Working    | Builds a tree level-by-level and reports info about coverage. |
| `draw_tree.py`        | Working    | Makes an SVG or PNG of our Binary Tree structure. |
| `integer_table.py`    | Working    | Builds a table in multiple bases, with steps, and odd-even chains. |
| `junk`                | Special    | Debugging cpp for quick one-off testing.  You can ignore it. |
| `peak_by_bit`         | Working    | Finds overflows in Collatz sequences for `2^bit` size.  GPU augment tested on an RTX-5060. |
| `performance_stats`   | Working    | Debugging tool for analyzing speed and memory performance on a system. |
| `pow2_under_pow3.py`  | Working    | Emits a copy/paste-friendly table for the `Exponent::MAX_POW2_UNDER_POW3[]` array. |
| `s1_s2_separator.py`  | Working    | Breaks tree levels into S1 and S2 sets, showing patterns and deltas supporting tree behavior. |
| `single_collatz`      | Working    | Builds a single sequence and emits some stats about it.  Very simple. |
| `sieve_stuff`         | Unfinished | Tool to measure effects of an LRU to aid in generic sieve. |
| `step_counter`        | Unfinished | Tool to analyze steps and organize them. |
| `stride_math.py`      | Working    | Emits bit requirements for affine stride coefficients. |


# Build, Requirements, and Environment

While other compilers, platforms, and standards might work, we have only built and tested using:

* GCC and Clang compilers.
* Linux (specifically, Ubuntu 24.04)
* C++20
* jemalloc via LD_PRELOAD (recommended, not required)
* Libraries as defined in CMakeLists (OMP, GMP, etc)
* Libraries cloned via `rebuild.sh`

The build system is CMake and should be (semi) easy-to-use.  The `rebuild.sh` script helps clone repos, link things, and execute
`cmake` to build and link programs.  If you're a build expert and want to make an MR to make this smoother for others, please do.

Once built, you can execute `tests.sh` to run all the unit/regression/whatever test programs.  This too could use polish.
