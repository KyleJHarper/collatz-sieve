# Harper's Sieve of Collatz

__A Monotonic Reduction of the Collatz Search Space__

# Overview

An implementation of the data structure and sieve outlined in Kyle Harper's analysis and optimization of the
[Collatz Conjecture's](https://en.wikipedia.org/wiki/Collatz_conjecture) problem space.

The code builds a binary tree in a unique, deterministic manner which allows the classification of parents and children into parity
vectors, called __F-G Chains__.  These chains identify __High-Water Mark__ nodes whose parity vectors prove contractive for itself
and all descendants, creating _coverage_ of a congruence class equivalent to a subtree.  The result leaves less than 1% of the
search space ℕ after 33 levels.  Such a tree can be built in a few seconds on a desktop CPU, requiring only ~40MB of memory.
Larger trees are possible by its monotonic nature, using more resources, and growing closer to 100% coverage (though never 100%).
The following table shows coverage and performance when building a tree using `uint64_t` on a desktop PC (i5-14600, DDR5).

| Tree Levels | Coverage | Time (sec) | Size (MB) |
| ----------: | -------: | ---------: | --------: |
|          10 |   92.58% |         <1 |        <1 |
|          20 |   97.14% |         <1 |        <1 |
|          30 |   98.81% |         <1 |         3 |
|          35 |   99.12% |          2 |        52 |
|          40 |   99.34% |         50 |      1100 |

The `BinaryTree` at the core Harper's Sieve is a sieve in the truest sense: it filters out entire classes of integers from needing
verification.  The _coverage_ listed above is achieved by the sieve alone.  That said, it does not preclude other techniques to
further reduce space or accelerate verification steps.  Mod-3 tables, stride tables, stopping-time (High-Water Mark) shortcuts, and
other techniques can (and should) be leveraged during the verification phase.

This implementation is written in C++ and supports fixed-widths up to 128 bits, as well as arbitrary precision via
[GMP](https://gmplib.org/).  It assumes your system is capable of 64-bit support, especially `uint64_t`, and `__int128` found in
GCC and Clang.

Starting with version 3.x, we attempt to follow [SemVer](https://semver.org/).

Data (trees) created with this program can be exported and imported across platforms.

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

### Data Types and Cost Model

The API accepts any native, fixed-width type up to 128 bits, such as `uint8_t`, `uint16_t`, etc.  GCC/Clang's 128-bit type has been
typedef'd to `uint128_t` for convenience.  The API expects an unsigned type.

Do not use implicit-width types (`size_t` and `uint`) if you plan to export/import trees.  They are, by definition, not portable.

The API supports >128 bits with GMP's `mpz_class`.  It has been highly optimized, but is still 2-5x slower than native types for
the same bit size.

Overall, the design philosophy is built as a cost model: smaller types are faster, larger types allow larger ranges for trees and
testing.  As expected, types which align with your CPU's bit size are fastest (e.g.: `uint64_t` on a 64-bit CPU).  Larger types
require more instructions (limbs, chunks, etc).

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

### Save and Load

Several classes were given `serialize()` and `deserialize()` methods, and the `BinaryTree` facade was given a `save()` and `load()`
method too.  A lot nuance is involved in this, so read the following sections carefully before using it.

#### Portable

The file emitted is a rudimentary, binary format.  It mostly serializes integrals and booleans, and the `CRoaring` bitmap objects
as needed.  A `StreamHelper` class ensures endianness remains little, however I don't have access to a big-endian system to test
this. As for the `CRoaring` object, we invoke the portable version of their export.

All-in-all, trees exported on different systems *should* work equally on any platform.

#### Type Consistency and Promotion

Currently, exported trees can only be imported to identically-sized types.  For example, if you build a `BinaryTree<uint64_t>` and
export it, you cannot load it into a `BinaryTree<uint128_t>`.  This is due to `sizeof(T)` being an implicit assumption inside the
methods like `StreamHelper::serialize_integral()`, along with custom pathing for GMP's `mpz_class`.

An item is on the TODO list for type promotion, but currently isn't available.

#### Output and Compression

The `tree.save(...)` method accepts a `path`, which it expects to map to a file or file-like object (`std::ofstream` internally).
Following the design of many GNU programs, you may specify a single hyphen to write to stdout: `tree.save("-")`.  Note, this will
map output to `&std:cout`.  We also perform a simple `isatty()` to prevent writing binary data to terminal.  The `tree.load()`
method will also accept a hyphen to read from stdin: `tree.load("-")`.

The `tree.save(...)` method will compress your tree with [ZStandard](https://github.com/facebook/zstd) by default, using the
maximum compression, equivalent to CLI: `zstd --ultra -22`.  To improve multithreading performance, we start with the end directive
`ZSTD_e_continue` and swtich to `ZSTD_e_flush` when `compression_level > 19`, because the window size simply gets too big to engage
more threads at higher compression.

If you dislike any of the compression choices, you can always write to stdout and pipe it into `zstd` CLI yourself, using whatever
parameters you prefer.  The `save()` and `load()` methods create and read *any* correctly formatted ZStandard file.

Finally, if you don't want compression for whatever reason, you can specify zero: `tree.save(path, 0)`.  This will bypass ZStandard
entirely and write in raw format to disk.

Both Materialized and Implicit trees compress well.  Above level ~32, compression tends toward ~2-3% (97-98% reduction).  So yeah,
we REALLY REALLY recommend you use compression.

Compression Results Table (Implicit Tree, Level 40, 64-bit type 1GB Raw)

| Zstd Level | Raw Size (MB) | Comp Size (MB) | Ratio  |
| ---------: | ------------: | -------------: | -----: |
|          1 |          1030 |            234 | 22.23% |
|          2 |          1030 |            235 | 22.37% |
|          3 |          1030 |            214 | 20.35% |
|          4 |          1030 |            213 | 20.24% |
|          5 |          1030 |            188 | 17.91% |
|          6 |          1030 |            183 | 17.43% |
|          7 |          1030 |            170 | 16.16% |
|          7 |          1030 |            166 | 15.78% |
|          9 |          1030 |            155 | 14.79% |
|         10 |          1030 |            142 | 13.52% |
|         11 |          1030 |            131 | 12.47% |
|         12 |          1030 |            131 | 12.45% |
|         13 |          1030 |            133 | 12.68% |
|         14 |          1030 |            119 | 11.33% |
|         15 |          1030 |            109 | 10.34% |
|         16 |          1030 |            104 |  9.91% |
|         17 |          1030 |             84 |  8.03% |
|         18 |          1030 |             82 |  7.77% |
|         19 |          1030 |             77 |  7.36% |
|         20 |          1030 |             43 |  4.11% |
|         21 |          1030 |             30 |  2.86% |
|         22 |          1030 |             21 |  1.97% |

#### Materialized Saving is a Bad Idea

Saving and loading materialized trees is supported **but not encouraged**.  It's almost always faster to rebuild a tree than load
an existing one.  However, the whole point of the `MaterializedBinaryTreeImpl` is to give a real, node-instantiated binary tree for
review.  Therefore, we support it with one caveat: you can **NOT** have any pruning enabled.  While technically possible, the logic
required to make this work is annoying as @#$% and I don't feel like doing it, especially when you can rebuild it in seconds.
Furthermore, be aware the size output is huge.  A simple 28-level tree with `zstd -19` compression requires 285MB (7.6GB raw).

# Build, Requirements, and Environment

While other compilers, platforms, and standards might work, we have only built and tested using:

* GCC and Clang compilers
* Linux
* C++20
* jemalloc via LD_PRELOAD (recommended, not required)
* Libraries as defined in CMakeLists (OMP, GMP, etc)

The build system is CMake and should be (semi) easy-to-use.  The `rebuild.sh` script helps invoke `cmake` in sensible ways, but you
may perform this manually of course.

Once built, you can execute `tests.sh` to run all the unit/regression/whatever test programs.
