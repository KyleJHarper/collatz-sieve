# Overview

__System Build for All Tests__

* Ubuntu 24.04 64-bit, Desktop Version
* Intel Core i3-4160 CPU (2 Core, [Intel Spec Sheet](https://www.intel.com/content/www/us/en/products/sku/77488/intel-core-i34160-processor-3m-cache-3-60-ghz/specifications.html))
* 16GB RAM DDR-3 [Kingston Spec Sheet](https://www.kingston.com/dataSheets/HX316C10FBK2_8.pdf)
* RTX 5060 GPU [ASUS Spec Sheet](https://www.asus.com/us/motherboards-components/graphics-cards/dual/dual-rtx5060-o8g/techspec/)
* Donor System: Intel Core i5-14600K [Intel Spec Sheet](https://www.intel.com/content/www/us/en/products/sku/236799/intel-core-i5-processor-14600k-24m-cache-up-to-5-30-ghz/specifications.html) DDR5-6000CL30

Note: when larger memory and/or high core count was required, the donor system with more RAM was used and is noted.

# Version History

## 4.2.0

### Mutable Iterator Functors

The `NodeBitmap::for_each_value()` and `BinaryTree::for_each_uncovered_value()` and their with-TLS-variants no longer require the
callback to be `(const T&)`, it can simply be `(T&)`.  These iterators already reconstruct values from parts and are not affected
by mutations by the callee.  Since value reconstruction was already happening, this change has no performance impact.

Callers may, of course, continue to specify `const` if desired.

Note, the ref (&) is still required, to prevent `mpz_class` types from triggering alloc() calls with each callback.

### Peak By Bit Bug Fix

The `peak_by_bit` program had a bug which prevented it from promoting from 64-bit to 128-bit.  This was fixed.

### Cache Line Optimization (False Sharing)

Some thread-local structs were false sharing.  Specifically the implicit tree building and CPUVerifier loop.  This was fixed and
performance jumped considerably.  Implicit tree building of a level 40 tree went from 37s to 28s.

### CPU and GPU Verifiers

A `Verifier` superclass was built to take a `BinaryTree<T>` and begin verifying surviving values via the `CPUVerifier` or the
`GPUVerifier` implementations.  A full write-up is available in the README file.

### Tree Builder (Program)

A new program was written to help make building trees easier: `tree_builder`.  It does what it sounds like.  Options available by
calling `tree_builder -h`.

### Verifier (Program)

A new program was written which takes in a prebuilt tree (from `tree_builder`) and then begins verifying surviving values.  It
provides realtime updates to any ANSI-compliant terminal.  The program is called `verifier`.  Options are available with `-h`.

## 4.1.0

### Value Iterator

The `BinaryTree` class handles nodes by-position, not by-value.  This is how classification remains stable, and as a side effect,
the `BinaryTreeImplicitImpl` (default tree type) achieves staggering compression inside its `NodeBitmap`.  Each position in the
bitmap requires ~0.5 bytes.

To iterate over actual values, conversion is required.  The `BinaryTree` facade has a `generate_value_map()` which will perform a
bulk conversion of positions into values.  This data then lives within the tree, and will be exported/imported whenever `save()` or
`load()` is called (serialize/deserialize under the hood).  The data is far more sparse, but still impressive at ~2 bytes each,
regardless of the underlying data type.  The `generate_value_map()` method uses `ForEachPolicy::SERIAL` by default.  If you want
parallel value map building, you may, but the parallel version uses 3-5x more memory during benchmarking, and only speeds up build
time by ~2x, sometimes it's even slower than serial.

#### Iteration Is FAST

The `tree.for_each_uncovered_value()` is mostly a wrapper over the `NodeBitmap().for_each_value_with_tls()`.  As such, reads are
blazingly fast and low-memory due to prefix hoisting, locality, and iterating over CRoaring's internal containers directly.  Note
that the serial policy uses CRoaring's native iterator, which is slower than the parallel policy codepath which iterates containers
directly. As such, you should almost always use `ForEachPolicy::PARALLEL` even with just 1 thread, unless you truly need guaranteed
order.

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

#### Continuation

The iterator supports an additional parameter for a `start` value, which allows continuation from a given point.  It isn't an exact
value, but guarantees to be at or below the start.

## 4.0.0

### Code Refactor

All code was refactored to clarify intent and improve overall composition.  The most major change has been to the concepts for data
types and how they're leveraged.  There are two primary types: `FixedWidthIntegral` and `GMPIntegral`.  These give time-memory
trade-offs, and allow for a cost-based model, wherein larger types are generally slower and use more memory, but allow larger
values.

The `FixedWidthIntegral` includes anything that responds to `std::integral` as `true`, which covers all integer types up to 64
bits.  128-bit support is appended to the fixed-width concept with another concept: `Int128Integral`.  Your compiler must support
this, and this is currently only tested with GCC/clang's __int128.

Testing was done to fall back on `absl::uint128`, but there are hurdles to consider when doing this.  Due to time constraints, this
was discarded.  Regardless, the objective was to make extension easier.  Theoretically, it should possible to use any
fixed-width integral (e.g.: absl, boost) as long as it:

* Is truly fixed width and stack-allocated.
* Responds to arithmetic operators the same as standard integrals.
* Responds to bitwise operators the same as standard integrals, especially shifting and masking.
* Responds reasonably to promotion from equal or lower types.

In practice, however, there's `constexpr` juggling and gotchas that I simply don't have time for.

Finally, the `GMPIntegral` is a special code path which allows `mpz_class` as the type.  This type has no limit on digits, but is
much slower than fixed-width types, and only becomes competitive around 512+ bits.

Note, the decision to use `GMPIntegral` instead of wrapping it further inside something like `UnlimitedWidthIntegral` came down to
GMP's ubiquity and first-class performance.  Furthermore, supporting an additional arbitrary type such as GMP would require a major
refactor, not a simple tweak to the typedef's and concepts.  Ergo, if it's ever desired, the extension of the `GMPIntegral` concept
into another larger concept would be a trivial effort by comparison.

### BinaryTree Composition Changes

The `BinaryTree` class is now a static facade in front of the `BinaryTreeImplicitImpl` and `BinaryTreeMaterializedImpl` classes.
Runtime polymorphism was never needed, so the whole dynamic pattern was useless.  As a result, the `IBinaryTreeBackend` interface
was deleted entirely.

`ImplicitBinaryTree<T>` and `MaterializedBinaryTree<T>` were made as aliases for convenience.

### Implict Tree Default

The `BinaryTree` class now defaults to using `BinaryTreeImplicitImpl`, hence the 4.x version bump.  Unless you're planning to view
nodes in memory or something like that, there's no reason to make a materialized tree anymore.

### Save/Load/Equal (Serialization/Deserialization)

_You can now save and load!_

Several classes were given `serialize()` and `deserialize()` methods, and the `BinaryTree` facade was given a `save()` and `load()`
method too.  A lot nuance is involved in this, so read the following sections carefully before using it.

#### Portable

The file emitted is a rudimentary, binary format.  It mostly serializes integrals and booleans, and the `CRoaring` bitmap objects
as needed.  A `StreamHelper` class ensures endianness remains little, however I don't have access to a big-endian system to test
this. As for the `CRoaring` object, we invoke the portable version of their export.

All-in-all, trees exported on different systems *should* work equally on any platform.

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

Compression Results Table (Implicit Tree, Level 40, 1GB Raw)

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

#### Equality

To aid with save/load testing, an `equal()` method was added to all classes which have `serialize()` and `deserialize()`.


### Root Node Instantiation Moved

The `BinaryTree*Impl` classes built the `_root_node` in the `init()` method.  This was never a good idea, because it meant that a
default-constructed tree would have no root node even after `add_level()` was called.  It also meant the root node wouldn't be
added back in necessarily if `reset()` was called.

To resolve this, `add_level()` is now the authority to build the root node if needed, and juggle the coverage, level_map, and
uncovered positions as needed.

### Documentation (Doxygen)

All documentation has been converted to Doxygen format.

### Unit Testing Extension and Standardization

There is now a unit test nearly 1-to-1 for each method or function.  Additionally, more complex operations (e.g.: save->load) are
also included.  Tests follow a common pattern.  The additional testing adds significant rigor to the code, and as a result causes
the test suite to run slower.  A full Debug + Release run takes ~3-5 minutes on the donor system (i5-14600).

### Dependency Management Change

The `rebuild.sh` script no longer clones repos.  Instead, CMake calls to `FetchContent` handle external sources.

## 3.6.0

### Affine Strides

Profiling showed the hotspot to be in `Node::init()`, which makes sense.  Deeper down, the core bottleneck resided in processing
sequences to get the F-G chains.  Several optimizations were made and tested.  Chief among them was affine strides using lookup
tables.

Micro-testing showed ~8 steps per stride to be ideal, generally keeping things close to L1 cache and registers.  Such a table fits
within 256 entries (2^8) and each entry is of type `AffineStride::Stride`, which is 8 bytes, which is a standard WORD on modern
64-bit systems.  The `mpz_class` type might benefit from much larger strides since it's heap-allocated and such, but testing never
revealed such behavior.

Here are the results of a level-40 build again, compared to version 3.5.0.  Using donor system, 12 threads.

| Data Type | Time (3.5.0) | Time w/Strides |   Delta | Speed |
| :-------- | -----------: | -------------: | ------: | ----: |
| uint64_t  |        172 s |           47 s |  -125 s | ~3.7x |
| uint128_t |        183 s |           71 s |  -112 s | ~2.6x |
| mpz_class |        820 s |          601 s |  -219 s | ~1.4x |

The hotspot has now moved to the NodeBitmap, specifically the CRoaring bitmap operations itself.  Optimizations might still exist,
but we will move on to testing parallel performance to ensure scaling to 100s of CPUs is still efficient.

## 3.5.0

### Interval Revamp for Memory Reduction

We replaced `Interval<T>` logic with a roaring bitmap from [CRoaring](https://github.com/RoaringBitmap/CRoaring).  Previously, each
`Interval` would require 16 or 32 bytes for `uint64_t` and `uint128_t`, respectively.  GMP's `mpz_class` was ~32 bytes each too.
Now, a bitmap tracks node locations in either an array of type `uint16_t` for 2 bytes, a bitmap for 1 bit, or an RLE.  While the
performance is variable based on density and distribution, it's drastically better than `Interval`.

Internally, CRoaring bitmaps are limited to 32 bits, and their 64-bit version would still limit our tree depth.  Therefore, we
extended their bitmap logic with our own additional prefix, and wrapped it in a `NodeBitmap<T>` class.

```
Roaring Bitmap (2^32)    :  key (uint16_t) + suffix (uint16_t)
Roaring64 Bitmap (2^64)  :  key (uint64_t) + suffix (uint16_t)  # Key limited to 48 bits logically.
NodeBitmap<T> (unlimited):  prefix (T) + Roaring (uint16_t key + uint16_t suffix)
```

We gained both speed increase and memory reduction.  The following table shows the wall-clock time for `Interval` vs `NodeBitmap`.
We used the donor system and 12 threads for all tests.

| Levels | Data Type | Interval (MB) | CRoaring (MB) | Delta | Interval (sec) | CRoaring (sec) | Delta |
| -----: | --------: | ------------: | ------------: | ----: | -------------: | -------------: | ----: |
|      8 | uint64_t  |             9 |             9 |    0% |              0 |              0 |    0% |
|      8 | uint128_t |             9 |             9 |    0% |              0 |              0 |    0% |
|      8 | mpz_class |            13 |            13 |    0% |              0 |              0 |    0% |
|     16 | uint64_t  |            10 |            10 |    0% |              0 |              0 |    0% |
|     16 | uint128_t |            10 |            10 |    0% |              0 |              0 |    0% |
|     16 | mpz_class |            13 |            13 |    0% |              0 |              0 |    0% |
|     24 | uint64_t  |            16 |            14 |  -13% |              0 |              0 |    0% |
|     24 | uint128_t |            18 |            14 |  -22% |              0 |              0 |    0% |
|     24 | mpz_class |            21 |            16 |  -24% |              1 |              1 |    0% |
|     32 | uint64_t  |           204 |            37 |  -82% |              1 |              1 |    0% |
|     32 | uint128_t |           379 |            38 |  -90% |              1 |              1 |    0% |
|     32 | mpz_class |           340 |            38 |  -89% |              5 |              4 |  -20% |
|     40 | uint64_t  |        18,000 |         2,900 |  -84% |            174 |            141 |  -19% |
|     40 | uint128_t |        35,700 |         3,000 |  -92% |            183 |            158 |  -14% |
|     40 | mpz_class |       ~52,000 |         3,000 |  -94% |            820 |            668 |  -19% |

This resolves the memory explosion problem preventing us from scaling beyond level ~48.  We will push higher limits after we've
reviewed the FG-by-level-and-position optimization in our TODO list.

## 3.4.1

### Restructure of BinaryTree Files

The BinaryTree classes were a bit crammed into a single file, so these were moved around.

### Performance Regression

The `mpz_class` variation of the tree slowed a lot due to changes in `BinaryTreeMath`, specifically for `st_reverse_low_bits()`
and `st_node_value_by_position_and_level()`.  I created overloads which accept a `T& out` param.  Performance restored, however
~~the allocator is still the weakest link.  Even with `jemalloc`, we're limited by memory pressure, not CPU.  I can barely keep 2
cores busy.~~  This was a bad observation when using `heaptrack`.  Having it count allocs was the bottleneck.  The parallel
performance is still good.

~~I believe the majority of the overhead is in the `Interval` and the `std::vector` backing it.  These require a lot of setting and
splitting, which beats the allocator into the dirt.  There's a new change coming in 3.5.0 that should help with this, and we'll
re-evaluate then.~~  Allocator thrash is still an issue, and the 3.5.0 changes should help.

## 3.4.0

### Verification of Non High-Water Mark Nodes

When building a tree, the nodes which don't meet HWM (directly or by ancestor) need to be checked.  For example, the number `14` is
below the HWM because of the `G` step on node `2`.  The value becomes `7`.  That node isn't a HWM node, descendant of one, nor has
it been verified by the tree.  This leaves pockets of nodes that aren't verified, which means the overall High-Water Mark of the
tree isn't valid.

Work by [David Barina](https://link.springer.com/article/10.1007/s11227-025-07337-0) has externally verified up to `2^71` which is
the first node on level `72`, which further means nodes up to level `71` are verified.  As such, we'll do the following:
* Add optional `Collatz<T>::st_verify()` calls in `add_level()` for Non-HWM nodes.
* Leave the default `false` (aka: `BinaryTreeOptions{}.verify_non_hwm_nodes = false`)
* Add `IBinaryTreeBackend<T>::assert_level_verification(...)` calls in `add_level()`.
  * Throw error if level requested exceeds max (`71`) and verification is off.
* Add `disable_non_hwm_node_verification()` and `enable_non_hwm_node_verification()` to any `BinaryTree`.

The performance implication was heavy at first, nearly +300% the time to build a tree.  But by adding a local (OMP-local)
High-Water Mark tracker inside the hot loop, the performance hit was only +75% (level 38 took 70s instead of 40).  Obviously, the
performance remains the same if verification is disabled.

### High-Performance Verification

Oddly enough, I never finished writing a high-speed static verification.  There's now `Collatz<T>::st_verify()` and helpers to
verify a value as fast as possible and nothing more.  The `collatz_compression` needed some updating for testing, and I found GMP
received some big benefits from affine compression of ones-steps (0b..1111) due to alloc reduction I'm guessing.

It handles auto-upgrading to avoid overflows, which means the user can send any value of their given type, and we'll upgrade for
testing if needed behind the scenes.

### CollatzConstants

Moved `CollatzConstants` to its own file.  Should've always been this way.


## 3.3.0

### Bit Reversal Optimization on BinaryTree

The bulk of all work for both tree types in `add_level()` is the `Node::init()` logic.  Deeper down, this uses
`BinaryTreeMath::st_reverse_low_bits(bits)`.  Originally, we shifted bits around naively in a `for` loop, avoiding any intrinsics
or blackbox functions.  This became a hotspot and bottleneck, so support was added for both, and the compiler should select the
best version for a platform.

Time To Build 38-Level Tree _(Donor system for larger RAM Requirement, 12 cores)_

| Type      | Baseline | Blackbox | Builtin Intrinsic |
| --------: | -------: | -------: | ----------------: |
|  uint64_t |       44 |       33 |                32 |
| uint128_t |       56 |       37 |                36 |
| mpz_class |      220 | see note |          see note |

Note: the intrinsic `__builtin_bitreverse64()` was only operational via `clang++` on Ubuntu 24.04.  The baseline was tested with a
`clang++` build and found within +/-5%.  Additionally, I spent a few hours trying to find a way to perform bit reversal on a GMP
`mpz_class` type only to end up frustrated and slower than simply testing bits and assigning them.

## 3.2.0

### Affine Map Shortcut

The math turned out to be valid, so I added `CollatzAffineMapShortcut` and used it in `Node::init()`.  This made a minor (~5%)
performance boost for `uint64_t`, a moderate (~15%) performance boost for `uint128_t`, and a major (~35%) performance boost for
`mpz_class` (GMP).

All tests, including coverage, passed.  No issues.

### Larger Binary Tree per Bit Limit
Previously, the `BinaryTree` had a limited number of levels for fixed-width types (e.g.: `uint64_t`) because of this process:
* The `BinaryTree` adds levels via `add_level()`.
* Each level adds `Node` objects.
* The `Node` needs to compute its FG Chain, which requires stepping through Collatz sequences.
* Steps often grow (much) larger than `Node._value`, causing overflows of the node's `T`.

As a result, `Node` objects are limited to a peak-by-bit, which is the largest value in a Collatz sequence starting at N.  We have
a program which has mapped these out to ~109 places thus far.  However, an easier solution availed itself.

We now predict overflow in `Node::init()` for type `T` when processing `Collatz<T>::for_each_fg_chain_link`.  We called a helper
method using `template<AnySupportedIntegral U>`, which allows us to decouple the node's type `T` from the FG-chain generation using
type `U` briefly.

Additionally, the `CollatzAffineMapShortcut` class was updated to use a lookup table of precalculated maximal power-of-two
coefficients for any given power of 3 (up to 3^512).  This avoids type juggling; everything is just `size_t` now.

Combined, the overflow potential has disappeared outside of `Node._value`.  This means trees can now be built up to the bit width
of their type `T`, minus 1 because counting starts at 0, and minus 1 more if the type is signed.

| Type      | Old Limit | New Limit | Reason |
| :-------- | --------: | --------: | :----- |
| uint64_t  |        32 |        63 | Collatz sequence overflows at level 33. |
| uint128_t |        80 |       127 | 3^81 (`_threes_exp > 81`) overflows in affine map. |
| mpz_class |       n/a |       n/a | No limit. |

The result is the ability to use native types like `uint64_t` for much larger trees, keeping memory lower.  For example, we tested
a level 38 tree which required `uint128_t` in our last patch.  It used ~12.5GB (peak RSS) with ~3.7GB truly active.  Building the
same tree with `uint64_t` required half the memory: ~6.2GB (peak RSS) and ~1.9GB active.

An additional benefit is speed: the `uint64_t` version takes about 80% as long as the `uint128_t`.  Which, to be fair, is a huge
credit the compiler and hardware's ability to handle 128-bit types so well on a 64-bit system.

## 3.1.0

### Peak By Bit
The CPU-only version was superseded by the CPU/GPU version which can use either (but not both at once).

Improvements were made to the GPU version (always uint128_t):

| Rate (per sec) | Change | Description |
| -------------: | -----: | :-----------|
|  2,895,000,000 |   0.0% | Baseline before changes. |
|  2,996,000,000 |  +3.5% | Used `current_step & 1` instead of `current_step % 2` in `collatz_get_peak`. |
|  3,447,000,000 | +19.1% | Remove `if/else` for just `if` and handled implied `x/2` inherently. |
|  4,278,000,000 | +47.8% | Shift by `__ffs() - 1` (CTZ) instead of 1 to skip repeated `x/2` steps. |
|  4,311,000,000 | +48.9% | Only write to `d_peaks[]` when necessary.  Otherwise, use local var. |

Improvements were made to the CPU version, and a dedicated `Collatz<T>::st_get_peak_fast()` was built.
* For 64- and 128-bit integrals, performance went from 25,000,000/sec to approximately 400,000,000/sec.
* For GMP, performance went from 15,000,000/sec to approximately 140,000,000/sec.  Mostly from alloc() reduction.

### Affine Map
Removed all of the `tls_fg_*` stuff from `Node` and replaced it with `CollatzAffineMap`.  This resulted in a 2-3x boost in speed
for native and extended integrals, and a 20-30% boost for GMP.

### Implicit Tree RAM
The `BinaryTreeImplicit` implementation consumed too much memory because of the `Interval` tracking, which was copied between
levels and then duplicated several times over.

I performed a test with a 38 level tree and found the following optimizations.

| Technique                 | Memory RSS (GB) | Description |
| :------------------------ | --------------: | :---------- |
| None                      |            22.8 | Baseline before changes. |
| Single Covered Interval   |            17.3 | Only keep the latest covered interval, not one per level. |
| Track Uncovered Intervals |            12.5 | Track uncovered intervals, not covered.  Smaller set. Also merge intervals. |

__Note__: Bytes are RSS, not private in-use.  True bytes used spike near the table's values, but drop to ~3.7GB afterward.

### Draw Tree (Python Diagrammer)
Updated the Python version of the BinaryTree and Node classes to be 1-based so I could fix the `draw_tree.py` script.


## 3.0.0
New version to support a plethora of changes, namely the change to an implicit tree.

Optimizations were applied in several areas related to parallel workload, tracking, and memory management.  Special care was taken
with OMP loops and tight loops to avoid blocking and barriers, and to spread load evenly while ensuring `thread_local` storage
(TLS) alleviated allocation overhead.  Trees (_implicit_ trees) are now built with only a few hundred allocs().

### Math Pass
While functional, some of the math was confusing because it started level numbering at 0 instead of 1.  This made sense from a
programming standpoint (i.e. indexing) but created a problem: positions are 1-based but levels are 0-based.  The code was updated
to treat levels as 1-based.

### Implicit Tree (Memory Reduction)
The `BinaryTree` class was turned into a facade with an interface called `IBinaryTreeBackend`.  The original logic which created
nodes in memory was renamed to `BinaryTreeMaterialized` and a new tree was added called `BinaryTreeImplicit`.  Both implementations
use `BinaryTreeMath` and provide an equivalent represenation of the data structure for our research.

In summary, the implicit tree doesn't generate `Node` objects for long-term storage.  Instead, it generates node values by level
and position via `BinaryTreeMath<T>::st_node_value_by_position_and_level()`.  It then stores `Intervals` which represent subsets of
the tree that have been covered by a high-water mark ancestor.  This creates a mathematical representation of the data structure
created when we build a materialized tree.

It is slightly slower, but uses drastically less memory, allowing for much deeper level building.

#### CPU -- Materialized w/Pruning vs Implicit

CPU time grows linearly, averaging ~10% slower for Implicit vs Materialized trees.

| Tree Levels | Data Type | Materialized (ms, 1 thr) | Implicit (ms, 1 thr) |
| ----------: | :-------- | -----------------------: | -------------------: |
|           8 | uint64_t  |                        0 |                    0 |
|          12 | uint64_t  |                        0 |                    0 |
|          16 | uint64_t  |                        2 |                    2 |
|          20 | uint64_t  |                       37 |                   40 |
|          23 | uint64_t  |                      272 |                  295 |
|           8 | uint128_t |                        0 |                    0 |
|          12 | uint128_t |                        0 |                    0 |
|          16 | uint128_t |                        3 |                    3 |
|          20 | uint128_t |                       41 |                   45 |
|          23 | uint128_t |                      300 |                  304 |
|           8 | mpz_class |                        0 |                    0 |
|          12 | mpz_class |                        0 |                    0 |
|          16 | mpz_class |                        4 |                    5 |
|          20 | mpz_class |                       63 |                   72 |
|          23 | mpz_class |                      422 |                  492 |

#### RAM -- Materialized w/Pruning vs Implicit

Memory is increasingly smaller (vanishing) for Implicit vs Materialized as levels grow.

| Tree Levels | Data Type | Materialized (bytes) | Implicit (bytes) |
| ----------: | :-------- | -------------------: | ---------------: |
|           8 | uint64_t  |                1,664 |            1,464 |
|          12 | uint64_t  |                9,248 |            3,224 |
|          16 | uint64_t  |               84,288 |           21,112 |
|          20 | uint64_t  |              960,992 |          119,640 |
|          23 | uint64_t  |            5,968,008 |          775,168 |
|           8 | uint128_t |                1,896 |            2,120 |
|          12 | uint128_t |               10,464 |            5,480 |
|          16 | uint128_t |               94,904 |           40,840 |
|          20 | uint128_t |            1,081,232 |          237,736 |
|          23 | uint128_t |            6,714,152 |        1,548,672 |
|           8 | mpz_class |                1,896 |            2,120 |
|          12 | mpz_class |               10,464 |            5,480 |
|          16 | mpz_class |               94,904 |           40,840 |
|          20 | mpz_class |            1,081,232 |          237,736 |
|          23 | mpz_class |            6,714,152 |        1,548,672 |

### Coverage Now Supports 128-Bit Integral
The `coverage` program now supports `uint128_t`.  It will auto-upgrade from `uint64_t` if the requested level exceeds the max for a
tree of that type.  The auto-upgrade to `mpz_class` works too, but at this point I've only calculated 2^109, so it's premature.

### Forward-Looking Cache (LRU Cache) on Sieve
A basic `ForwardLookingCache` was created and is available to the sieve class, but it hasn't been implemented.  The sieve also has
known bugs, particularly with OMP, and has hence been commented out.  The addition of an Implicit tree further changes how a sieve
should be built, because there is no `level_map` anymore.  We will probably hide that behind a shared item in the IFace and tell
the sieve to use it instead of talking directly to a "map".

## 2.1.0
Sieve works and produces values based on the survivors of a BinaryTree.  Supports batch `next()`.

## 2.0.0
BinaryTreeMath is now rooted at 1 by default.  This is a major breaking change, hence the version bump.

All the python tools are going to need to be adjusted, or retired if not needed anymore.

The major fix will be to the tools which create, walk, or diagram trees (e.g.: tools/draw_tree.py).

## 1.4.0
The BinaryTree and its associated BinaryTreeMath now support building a 1-based (root node == 1) tree.  This wasn't necessary for
my purposes, but it aligns with how people often view perfect binary trees.  It has zero effect on my coverage or testing, so I
will likely switch to it as the default in an upcoming version.

Important Notes:
* Coverage doesn't change.  The only way it would've, is if an edge node became a high-water mark ancestor.  This isn't possible
with any value other than 2.  Since 2 remains on level 1 (right-hand side in 0-based, left-hand side in 1-based), nothing changes.
* Unit tests were expanded extensively to support integral, MPZ, 0-based, and 1-based roots, and all combinations thereof.
* A few tweaks were needed in our programs, but only to rely on BinaryTreeMath.  E.g.: starting at "base level" instead of assuming
all nodes start at level 1 (from the 0-based original approach).
* The ancestor searching provides the same results: coverage of an ancestor happens at the same descendant regardless of 0-based or
1-based tree structure, which was expected but nice to see.

## 1.3.0
Moved all the math for the BinaryTree into BinaryTreeMath.  This helped decouple Node from BinaryTree, and provides a cleaner way
to handle the math across all programs and classes.

Discovered that we can use the reversal of the least-significant bits in Node values and positions to translate between them.

```
Given:
static inline T st_reverse_low_bits(const T& value, size_t bits)
  ==> converts the last "bits" on the LSB side by reversing them (e.g.: 110 -> 011)
  ==> when related to Level, allows neat tricks with node values and positions
```

This allowed me to generate two important functions:

__Node Position By Value__
```
position(N) = reverse_bits_L((N+1) mod 2^L, L) + 1
```

__Node Value by Level and Position__
```
value(P,L) == 2^L + reverse_bits_L(P - 1, L) - 1
```

Note: I previously had a summation-style logic which used concepts called first_node_value, s1, and s2.  While mathematically
correct, they are a summation, and I prefered a more closed-form algebraic expression.  You can see the details of this logic in
the method `st_node_value_by_position_and_level__deprecated()`.

Also modified pruning to distinguish between HWM pruning (nodes and descendants) vs parent-level pruning.  This has no effect on
the `_coverage_map` within the tree at all.  You can still use `.node_count()` to get a real count of nodes in the vectors.

Extended support of binary_tree_class tests, because I ran into a huge headache hidden beneath a double-free. Tests are slower, but
will hammer the snot out of tree-building with all pruning types.

Confirmed that none of these changes affected the `performance_stats` output.  We're still speedy.

Started on an update for the Ancestor tracking, but I'm still in flux on what this will do.  Need to output a more meaninful table.

## 1.2.0
The high-water mark ancestors are now trackable in the BinaryTree.  It added about 1% overhead.

Fixed a value in the 1.1.0 table below.  I did a timing test on the wrong machine.  Fixed the CPU time.

## 1.1.0
When `prune` is enabled, the BinaryTree will also remove parent nodes at the end of `add_level()`.  This cuts the tree in half
(RAM) without affecting the `_coverage_map` data.

| Version | Data Type | RAM (bytes) | RSS (bytes) | CPU (ms, 1 thr) |
| :------ | :-------- | ----------: | ----------: | --------------: |
| 0.8.1   | uint64_t  |   8,389,976 |   9,830,400 |             127 |
| 1.0.0   | uint64_t  |     322,584 |     524,288 |               6 |
| 1.1.0   | uint64_t  |     136,728 |     393,216 |               6 |
| 0.8.1   | mpz_class |   9,438,816 |  10,747,904 |             187 |
| 1.0.0   | mpz_class |     363,000 |     393,216 |               8 |
| 1.1.0   | mpz_class |     153,912 |     393,216 |               8 |

## 1.0.0
The first full implementation of my algorithm!

BinaryTree now supports a `prune` option, which will remove nodes that hit the high-water mark.
This is a real-time implementation of my strategy, with staggering results.  Since the building of
objects and overall program speed is linear, the reduction of nodes is giving equivalent gains in
memory usage and CPU time.  Here are the final resource values compared with 0.8.1:

| Version | Data Type | RAM (bytes) | RSS (bytes) | CPU (ms, 1 thr) |
| :------ | :-------- | ----------: | ----------: | --------------: |
| 0.8.1   | uint64_t  |   8,389,976 |   9,830,400 |             127 |
| 1.0.0   | uint64_t  |     322,584 |     524,288 |               6 |
| 0.8.1   | mpz_class |   9,438,816 |  10,747,904 |             187 |
| 1.0.0   | mpz_class |     363,000 |     393,216 |               8 |

Below is the evidence that at just level 32 (2^32) we've reduced the test space by 99%:
```
kyle@green-machine:~/Desktop/repos/3n1 (dev)$ bin/coverage -l 32 -v
[2025-08-12 19:16:38] [debug] Selected options were:
[2025-08-12 19:16:38] [debug]   Force MPZ: false
[2025-08-12 19:16:38] [debug]   Levels: 32
[2025-08-12 19:16:38] [debug]   Verbose: true
[2025-08-12 19:16:38] [info] Building tree with 32 levels, using uint64_t.
[2025-08-12 19:16:38] [debug] Building level 1...
[2025-08-12 19:16:38] [debug] Level 1 coverage was: 50.0000% (1/2)
[2025-08-12 19:16:38] [debug] Building level 2...
[2025-08-12 19:16:38] [debug] Level 2 coverage was: 75.0000% (3/4)
[2025-08-12 19:16:38] [debug] Building level 3...
[2025-08-12 19:16:38] [debug] Level 3 coverage was: 75.0000% (6/8)
[2025-08-12 19:16:38] [debug] Building level 4...
[2025-08-12 19:16:38] [debug] Level 4 coverage was: 81.2500% (13/16)
[2025-08-12 19:16:38] [debug] Building level 5...
[2025-08-12 19:16:38] [debug] Level 5 coverage was: 87.5000% (28/32)
[2025-08-12 19:16:38] [debug] Building level 6...
[2025-08-12 19:16:38] [debug] Level 6 coverage was: 87.5000% (56/64)
[2025-08-12 19:16:38] [debug] Building level 7...
[2025-08-12 19:16:38] [debug] Level 7 coverage was: 89.8438% (115/128)
[2025-08-12 19:16:38] [debug] Building level 8...
[2025-08-12 19:16:38] [debug] Level 8 coverage was: 92.5781% (237/256)
[2025-08-12 19:16:38] [debug] Building level 9...
[2025-08-12 19:16:38] [debug] Level 9 coverage was: 92.5781% (474/512)
[2025-08-12 19:16:38] [debug] Building level 10...
[2025-08-12 19:16:38] [debug] Level 10 coverage was: 93.7500% (960/1024)
[2025-08-12 19:16:38] [debug] Building level 11...
[2025-08-12 19:16:38] [debug] Level 11 coverage was: 93.7500% (1920/2048)
[2025-08-12 19:16:38] [debug] Building level 12...
[2025-08-12 19:16:38] [debug] Level 12 coverage was: 94.4824% (3870/4096)
[2025-08-12 19:16:38] [debug] Building level 13...
[2025-08-12 19:16:38] [debug] Level 13 coverage was: 95.5200% (7825/8192)
[2025-08-12 19:16:38] [debug] Building level 14...
[2025-08-12 19:16:38] [debug] Level 14 coverage was: 95.5200% (15650/16384)
[2025-08-12 19:16:38] [debug] Building level 15...
[2025-08-12 19:16:38] [debug] Level 15 coverage was: 96.0480% (31473/32768)
[2025-08-12 19:16:38] [debug] Building level 16...
[2025-08-12 19:16:38] [debug] Level 16 coverage was: 96.7743% (63422/65536)
[2025-08-12 19:16:38] [debug] Building level 17...
[2025-08-12 19:16:38] [debug] Level 17 coverage was: 96.7743% (126844/131072)
[2025-08-12 19:16:38] [debug] Building level 18...
[2025-08-12 19:16:38] [debug] Level 18 coverage was: 97.1409% (254649/262144)
[2025-08-12 19:16:38] [debug] Building level 19...
[2025-08-12 19:16:38] [debug] Level 19 coverage was: 97.1409% (509298/524288)
[2025-08-12 19:16:38] [debug] Building level 20...
[2025-08-12 19:16:38] [debug] Level 20 coverage was: 97.3938% (1021248/1048576)
[2025-08-12 19:16:38] [debug] Building level 21...
[2025-08-12 19:16:38] [debug] Level 21 coverage was: 97.7774% (2050541/2097152)
[2025-08-12 19:16:38] [debug] Building level 22...
[2025-08-12 19:16:38] [debug] Level 22 coverage was: 97.7774% (4101082/4194304)
[2025-08-12 19:16:38] [debug] Building level 23...
[2025-08-12 19:16:38] [debug] Level 23 coverage was: 97.9877% (8219801/8388608)
[2025-08-12 19:16:38] [debug] Building level 24...
[2025-08-12 19:16:38] [debug] Level 24 coverage was: 98.2918% (16490635/16777216)
[2025-08-12 19:16:38] [debug] Building level 25...
[2025-08-12 19:16:39] [debug] Level 25 coverage was: 98.2918% (32981270/33554432)
[2025-08-12 19:16:39] [debug] Building level 26...
[2025-08-12 19:16:40] [debug] Level 26 coverage was: 98.4542% (66071490/67108864)
[2025-08-12 19:16:40] [debug] Building level 27...
[2025-08-12 19:16:42] [debug] Level 27 coverage was: 98.6870% (132455435/134217728)
[2025-08-12 19:16:42] [debug] Building level 28...
[2025-08-12 19:16:45] [debug] Level 28 coverage was: 98.6870% (264910870/268435456)
[2025-08-12 19:16:45] [debug] Building level 29...
[2025-08-12 19:16:52] [debug] Level 29 coverage was: 98.8106% (530485275/536870912)
[2025-08-12 19:16:52] [debug] Building level 30...
[2025-08-12 19:17:06] [debug] Level 30 coverage was: 98.8106% (1060970550/1073741824)
[2025-08-12 19:17:06] [debug] Building level 31...
[2025-08-12 19:17:34] [debug] Level 31 coverage was: 98.8991% (2123841570/2147483648)
[2025-08-12 19:17:34] [debug] Building level 32...
[2025-08-12 19:18:26] [debug] Level 32 coverage was: 99.0373% (4253619813/4294967296)
[2025-08-12 19:18:26] [info] Global Coverage: 98.9325% (8498236424/8589934590)
```

## 0.8.1
Final allocation changes are made.  We reduced the GMP copying and reallocations.  Here are the results:

| Version | Data Type | RAM (bytes) | RSS (bytes) | CPU (ms, 1 thr) |
| :------ | :-------- | ----------: | ----------: | --------------: |
| 0.5.1   | uint64_t  |  45,662,928 |         n/a |             245 |
| 0.6.0   | uint64_t  |  85,173,336 |  60,030,976 |             282 |
| 0.8.0   | uint64_t  |  14,681,376 |  14,942,208 |             135 |
| 0.8.1   | uint64_t  |   8,389,976 |   9,830,400 |             127 |
| 0.5.1   | mpz_class |  48,808,632 |         n/a |             865 |
| 0.6.0   | mpz_class | 114,115,904 |  66,453,504 |             887 |
| 0.8.0   | mpz_class |  15,730,216 |  18,087,936 |             241 |
| 0.8.1   | mpz_class |   9,438,816 |  10,747,904 |             187 |

And here's the final allocation data:

| Version | Data Type        | Allocations | For GMP   | Temp      |
| :------ | :--------------- | ----------: | --------: | --------: |
| 0.5.1   | Node<uint64_t>*  |   1,462,735 |   786,477 |   131,122 |
| 0.6.0   | Node<uint64_t>   |   2,158,447 | 1,441,832 |   131,123 |
| 0.8.0   | Node<uint64_t>*  |          79 |        47 |        17 |
| 0.8.1   | Node<uint64_t>*  |          77 |        47 |        17 |
| 0.5.1   | Node<mpz_class>* |   7,566,388 | 6,759,076 | 4,924,168 |
| 0.6.0   | Node<mpz_class>  |   8,393,170 | 7,545,501 | 4,924,169 |
| 0.8.0   | Node<mpz_class>* |     665,703 |   655,636 |   131,138 |
| 0.8.1   | Node<mpz_class>* |     131,422 |   131,355 |        68 |

## 0.8.0

Major rework on the memory layout and usage in Collatz and Node.

### Memory Usage Improved

* Metadata is stored separately, and is per-instance, up to the caller if they want it.
* Memory fits in a single cache line for Collatz (both) and Node<uint64_t>.  Only Node<mpz_class> remains.
* All temporary variables were removed:
  * Use of thread_local creates scratch-pads for temporary mpz/mpf items.
  * Use of mpz_*() methods avoid arithmetic operators which use temp vars.
  * Passing by reference (const T& x), helps.  This is now enforced in for_each() methods with callbacks.
* A bug in coverage was found and fixed (we passed by value, not reference).

### Tree Building Performance

The tree builds much faster now, as shown in this table

| Version | Data Type | RAM (bytes) | RSS (bytes) | CPU (ms, 1 thr) |
| :------ | :-------- | ----------: | ----------: | --------------: |
| 0.5.1   | uint64_t  |  45,662,928 |         n/a |             245 |
| 0.6.0   | uint64_t  |  85,173,336 |  60,030,976 |             282 |
| 0.8.0   | uint64_t  |  14,681,376 |  14,942,208 |             135 |
| 0.5.1   | mpz_class |  48,808,632 |         n/a |             865 |
| 0.6.0   | mpz_class | 114,115,904 |  66,453,504 |             887 |
| 0.8.0   | mpz_class |  15,730,216 |  18,087,936 |             241 |


### Allocation Performance Improved

Here is an updated table running `bin/coverage 16`:

| Version | Data Type        | Allocations | For GMP   | Temp      |
| :------ | :--------------- | ----------: | --------: | --------: |
| 0.5.1   | Node<uint64_t>*  |   1,462,735 |   786,477 |   131,122 |
| 0.6.0   | Node<uint64_t>   |   2,158,447 | 1,441,832 |   131,123 |
| 0.8.0   | Node<uint64_t>*  |          79 |        47 |        17 |
| 0.5.1   | Node<mpz_class>* |   7,566,388 | 6,759,076 | 4,924,168 |
| 0.6.0   | Node<mpz_class>  |   8,393,170 | 7,545,501 | 4,924,169 |
| 0.8.0   | Node<mpz_class>* |     665,703 |   655,636 |   131,138 |

Note: further reduction is possible if we can get Node and Collatz to store T `_value` and `_initial_value`
as references without ending up with dangling pointers/invalid references.

## 0.7.0

Refactored the Makefile to be a little better.  We can parallel build now.

Started profiling and we're doing okay with padding, alignment, and so forth, but there's a lot
of waste too with size_t in areas and bools not bit-packed.

Added better CLI support with CLI11.  It's fat, but convenient.

Added a global logger from spdlog.

At this piont I'm not sure better speed is a goal.  Our problems are now in memory, both with size and
allocation.  Resolving these gives more speed and space to work.

## 0.6.1

Reverted to to `std::vector<size_t, std::vector<Node<T>*>>` and threw the SlabAllocator away.

## 0.6.0

Switched from `std::vector<size_t, std::vector<Node<T>*>>` to `std::vector<size_t, std::vector<Node<T>>>` to avoid so many
allocations when building the BinaryTree.

### Goals

* Improve locality by calling `.reserve()` and `.resize()` with `child_count` since it's a deterministic value.
* Reduce allocations by storing Node objects as values instead of pointers to heap.

### Results
* Allocations increased, probably because `Node<T>` has allocations inside it anyway.  Namely, `mpz_class` and `mpf_class`.
* OMP stopped threading the work, probably because it doesn't believe that assignment to the vector is thread safe.
  * I'm an idiot and had the `#pragma omp parallel...` part commented out.  Performance is fine.
* Single-threaded performance remained the same, despite more allocations.

I executed `bin/coverage 16` to obtain the following:

| Version | Data Type        | Allocations | For GMP   |
| :------ | :--------------- | ----------: | --------: |
| 0.5.1   | Node<uint64_t>*  |   1,462,735 |   786,477 |
| 0.6.0   | Node<uint64_t>   |   2,158,447 | 1,441,832 |
| 0.5.1   | Node<mpz_class>* |   7,566,388 | 6,759,076 |
| 0.6.0   | Node<mpz_class>  |   8,393,170 | 7,545,501 |

### Conclusion

The loss of parallel construction is unacceptable.  We should either switch back to `Node<T>*` or build a slab allocator.

## 0.5.1

This is the last build before switching to value-based Node storage in BinaryTree.  This is mostly so we can compare performance
changes, namely those related to allocations and tree-building time.

## 0.5.0

Added detection inside of the Collatz class to handle overflow detection.  Runtime performance hit was minimal.

## 0.4.0

Switching away from `std::string` to `std::vector<bool>` for storing the `_oe_pattern`.  Note, the latter is a c++ specialized
template, which makes it act like a bitset (packed bits).

The performance gain was significant.  Memory usage was much lower.  We should probably use a real bitset, but I'm not sure how,
because bitsets generally require compile-time values ... but our sizes are dynamic.

## 0.3.0

BinaryTree now supports parallel construction.  Using OMP offered linear results with CPUs available, so I'm not going to worry
about a custom threader/worker system.  Note, we didn't get linear performance gains building the tree, just steady-state usage of
the CPUs (possible spin-waiting).  However, the gains are on par with parallel technology in general: you rarely get pure linear
performance.

## 0.2.0

First real version of the system in C++.  Using wayyy too much memory for what it's doing.  Python version used roughly 1-2GB, but
this one uses nearly 20GB somehow.  Much of it was tied to mpz_class (GMP), but also bad design decisions when prototyping.  We
need a performance stats program so we can track changes over time.

## 0.1.0

Beginning work on the conversion from Python programs in the tools/ directory to C/C++ in src/.
