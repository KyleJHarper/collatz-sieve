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

# Quick Start

Literally this:

```
#include "collatz/binary_tree.hpp"
#include "collatz/collatz.hpp"


int main() {

    BinaryTree<uint64_t> tree(32);
    tree.generate_value_map();
    tree.for_each_uncovered_value(ForEachPolicy::PARALLEL, [&](const uint64_t& value) {
        // Do whatever you want with "value" ... such as verify it.
        Collatz<uint64_t>::st_verify(value);

        // Let the iterator know it can continue.  Use ForEachSignal::BREAK to stop.
        if (value > (1ULL << 34)) {
            std::cout << "Reach a big enough value to be finished.  Value is: " << to_string_any(value) << std::endl;
            // ^ Race condition on purpose.  See below.
            return ForEachSignal::BREAK;
        }
        return ForEachSignal::CONTINUE;
    });

}
```

### Tree Size

Tree size is a time-memory trade-off.  Larger trees use more memory but the extra slivers of coverage improve verification speed.

### Serial and Parallel Policies

All iterators support a `ForEachPolicy` of `SERIAL` or `PARALLEL`.

When `SERIAL`, order is guaranteed and threading is disabled.  OMP is bypassed, not just set to "1" thread.  You may reference and
use variables willy-nilly outside your lambda/functor (i.e.: without guards).  However, this is slow and not recommended.

When `PARALLEL`, order is random-ish but confined to the range of a `NodeBitmap` prefix (2^32) or node scaling factor, which is
determined by your tree size and can be seen via `BinaryTreeMath<T>::st_scaling_factor(level)`.  It's faster, but requires the same
safeguards as any parallel code.  See next section for TLS.

### Thread-Local Storage  (AKA: Parallel Operation Demands Respect)

The example above writes to `std::cout` without a guard.  It's a race condition.  Callers often need more sophisticated controls
than simply writing to stdout.  To avoid barriers, there is a TLS-aware iterator which accepts a vector of whatever data type the
caller needs.  Like so:

```
#include "collatz/binary_tree.hpp"
#include "collatz/collatz.hpp"


int main() {

    struct CoolMetadata {
        uint64_t total_processed = 0;
        uint64_t skipped = 0;
        SomeCacheSystem cache;
    };
    BinaryTree<uint64_t> tree(32);
    tree.generate_value_map();
    std::vector<CoolMetadata> tls;
    tree.for_each_uncovered_value_with_tls(ForEachPolicy::PARALLEL, tls, [&](const uint64_t& value, CoolMetadata& my_tls) {
        // Check the cache first.
        if (my_tls.cache.contains(value)) {
            my_tls.skipped++;
        } else {
            Collatz<uint64_t>::st_verify(value);
            my_tls.cache.add(value);
        }

        // Bump count, even if it was skipped.
        my_tls.total_processed++;

        // Let the iterator know it can continue.  Use ForEachSignal::BREAK to stop.
        if (value > (1ULL << 34)) {
            return ForEachSignal::BREAK;
        }
        return ForEachSignal::CONTINUE;
    });

}
```

Notice, the vector will be resized (`.resize()`) automatically if it's too small for the thread count.  No other manipulation is
made.

### Iteration is FAST

The `tree.for_each_uncovered_value()` and its TLS-aware variant are mostly wrappers over `NodeBitmap().for_each_value_with_tls()`.
As such, reads are blazingly fast and low-memory due to prefix hoisting, locality, and iterating over CRoaring's internal
containers directly.  Note that the serial policy uses CRoaring's native iterator, which is slower than the parallel policy
codepath which iterates containers directly. As such, you should almost always use `ForEachPolicy::PARALLEL` even with just 1
thread, unless you truly need guaranteed order.

The `NodeBitmap` approach was tested extensively.  It outperforms raw memory (`malloc`) because raw memory uses 4-8x time the RAM
and hits memory bandwidth limits (and possibly pointer chasing) long before the CPU saturates.  Multithreading helped, but couldn't
outpace our `NodeBitmap`.  It also outperforms compressed memory.  The compressed memory was slightly smaller in size, and this
might scale at higher tree levels, but the decompression time was so massive it killed the overall throughput.

Here is a table showing the memory required for each test, and the throughput.  This was a level 37 tree, with 1,117,834,900
surviving positions/values.  Note, the c/ms (count per millisecond) is not a typo.  The system can provide billions of values per
second.

| Data Type | Policy   | Threads | Bitmap MB | Raw MB | Comp MB | Bitmap c/ms | Raw c/ms   | Comp c/ms |
| :-------- | :------- | ------: |---------: | -----: | ------: | ----------: | ---------: | --------: |
| uint64_t  | Serial   |       1 |     1,014 |  4,026 |     685 |     810,572 |  2,180,507 |   105,011 |
| uint64_t  | Serial   |       2 |     1,014 |  4,026 |     685 |     794,702 |  2,085,702 |   104,657 |
| uint64_t  | Serial   |       4 |     1,014 |  4,026 |     685 |     799,519 |  2,217,154 |   107,187 |
| uint64_t  | Serial   |       8 |     1,014 |  4,026 |     685 |     780,595 |  2,102,321 |   106,130 |
| uint64_t  | Serial   |      12 |     1,014 |  4,026 |     685 |     798,309 |  2,110,731 |   105,705 |
| uint64_t  | Parallel |       1 |     1,014 |  4,026 |     685 |   2,867,841 |  2,110,731 |   106,088 |
| uint64_t  | Parallel |       2 |     1,014 |  4,026 |     685 |   5,123,133 |  4,221,462 |   108,398 |
| uint64_t  | Parallel |       4 |     1,014 |  4,026 |     685 |   8,118,196 |  7,130,848 |   108,242 |
| uint64_t  | Parallel |       8 |     1,014 |  4,026 |     685 |  11,726,283 |  8,245,043 |   110,278 |
| uint64_t  | Parallel |      12 |     1,014 |  4,026 |     685 |  12,563,875 |  9,422,906 |   109,750 |
| uint128_t | Serial   |       1 |     1,014 |  8,052 |     687 |     725,835 |  1,319,206 |    80,219 |
| uint128_t | Serial   |       2 |     1,014 |  8,052 |     687 |     715,987 |  1,159,742 |    81,181 |
| uint128_t | Serial   |       4 |     1,014 |  8,052 |     687 |     719,894 |  1,296,517 |    79,006 |
| uint128_t | Serial   |       8 |     1,014 |  8,052 |     687 |     742,169 |  1,342,704 |    79,350 |
| uint128_t | Serial   |      12 |     1,014 |  8,052 |     687 |     709,251 |  1,188,474 |    84,052 |
| uint128_t | Parallel |       1 |     1,014 |  8,052 |     687 |   2,029,549 |  1,191,157 |    81,570 |
| uint128_t | Parallel |       2 |     1,014 |  8,052 |     687 |   3,404,404 |  2,145,051 |    82,360 |
| uint128_t | Parallel |       4 |     1,014 |  8,052 |     687 |   5,996,394 |  3,614,265 |    87,742 |
| uint128_t | Parallel |       8 |     1,014 |  8,052 |     687 |   8,245,043 |  4,510,109 |    87,524 |
| uint128_t | Parallel |      12 |     1,014 |  8,052 |     687 |   9,956,278 |  4,978,139 |    84,632 |

Note: `mpz_class` was not tested as extensively, but appears to iterate ~20x slower.  Roughly 180,000/ms.

### Continuation

The iterator supports an additional parameter for a `start` value, which allows continuation from a given point.  It isn't an exact
value, but guarantees to be at or below the start.

```
    uint64_t start = 1234567890;
    tree.for_each_uncovered_value_with_tls(ForEachPolicy::PARALLEL, tls, [&](const uint64_t& value, CoolMetadata& my_tls) {
        ...stuff...
    }, start);
```

### Alloc Choices

The code should work with any stable allocator, but [jemalloc](https://jemalloc.net/) gave the best results.

# Data Types and Cost Model

The API accepts any native, fixed-width type up to 128 bits, such as `uint8_t`, `uint16_t`, etc.  GCC/Clang's 128-bit type has been
typedef'd to `uint128_t` for convenience.  The API expects an unsigned type.

Do not use implicit-width types (`size_t` and `uint`) if you plan to export/import trees.  They are, by definition, not portable.

The API supports >128 bits with GMP's `mpz_class`.  It has been highly optimized, but is still 2-5x slower than native types for
the same bit size.

Overall, the design philosophy is built as a cost model: smaller types are faster, larger types allow larger ranges for trees and
testing.  As expected, types which align with your CPU's bit size are fastest (e.g.: `uint64_t` on a 64-bit CPU).  Larger types
require more instructions (limbs, chunks, etc).

# Classes & Facades

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

# Programs

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
| `step_counter`        | Unfinished | Tool to analyze steps and organize them. |
| `stride_math.py`      | Working    | Emits bit requirements for affine stride coefficients. |
| `verifier`            | Unfinished | Standalone Collatz verification as fast as possible. |

# Save and Load

Several classes were given `serialize()` and `deserialize()` methods, and the `BinaryTree` facade was given a `save()` and `load()`
method too.  A lot nuance is involved in this, so read the following sections carefully before using it.

### Portable

The file emitted is a rudimentary, binary format.  It mostly serializes integrals and booleans, and the `CRoaring` bitmap objects
as needed.  A `StreamHelper` class ensures endianness remains little, however I don't have access to a big-endian system to test
this. As for the `CRoaring` object, we invoke the portable version of their export.

All-in-all, trees exported on different systems *should* work equally on any platform.

### Type Consistency and Promotion

Currently, exported trees can only be imported to identically-sized types.  For example, if you build a `BinaryTree<uint64_t>` and
export it, you cannot load it into a `BinaryTree<uint128_t>`.  This is due to `sizeof(T)` being an implicit assumption inside the
methods like `StreamHelper::serialize_integral()`, along with custom pathing for GMP's `mpz_class`.

An item is on the TODO list for type promotion, but currently isn't available.

### Output and Compression

The `tree.save(...)` method accepts a `path`, which it expects to map to a file or file-like object (`std::ofstream` internally).
Following the design of many GNU programs, you may specify a single hyphen to write to stdout: `tree.save("-")`.  Note, this will
map output to `&std:cout`.  We also perform a simple `isatty()` to prevent writing binary data to terminal.  The `tree.load()`
method will also accept a hyphen to read from stdin: `tree.load("-")`.

The `tree.save(...)` method will compress your tree with [ZStandard](https://github.com/facebook/zstd) by default, using the
maximum compression, equivalent to CLI: `zstd --ultra -22`.  To improve multithreading performance, we start with the end directive
`ZSTD_e_continue` and swtich to `ZSTD_e_flush` when `compression_level > 19`, because the window size simply gets too big to engage
more threads at higher compression.

If you don't want compression for whatever reason, you can specify zero: `tree.save(path, 0)`.  This will bypass ZStandard entirely
and write in raw format to disk.

If you dislike any of the compression choices, you can always write to stdout and pipe it into `zstd` CLI yourself, using whatever
parameters you prefer.  The `save()` and `load()` methods create and read *any* correctly formatted ZStandard file.

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

### Materialized Saving is a Bad Idea

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
