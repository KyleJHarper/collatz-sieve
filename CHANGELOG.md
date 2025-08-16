# Overview

### Branching

Historically, all work was done in "master".  We will use "dev" for all development now.  We can feature branch if needed later.

### Tags

I prefer SemVer, but given the research nature of this project, I'm not sure it makes sense.  As such, we will use 0.x.y until a
better format avails itself.

I will tag things as new features are developed and list those features in the change log below.

# Change Log

__System Build for All Tests__
* Ubuntu 24.04 64-bit, Desktop Version
* Intel Core i3-4160 CPU (2 Core, [Intel Spec Sheet](https://www.intel.com/content/www/us/en/products/sku/77488/intel-core-i34160-processor-3m-cache-3-60-ghz/specifications.html))
* 16GB RAM DDR-3 [Kingston Spec Sheet](https://www.kingston.com/dataSheets/HX316C10FBK2_8.pdf)

### 1.3.0
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

### 1.2.0
The high-water mark ancestors are now trackable in the BinaryTree.  It added about 1% overhead.

Fixed a value in the 1.1.0 table below.  I did a timing test on the wrong machine.  Fixed the CPU time.

### 1.1.0
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

### 1.0.0
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

### 0.8.1
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

### 0.8.0
Major rework on the memory layout and usage in Collatz and Node.

#### Memory Usage Improved
* Metadata is stored separately, and is per-instance, up to the caller if they want it.
* Memory fits in a single cache line for Collatz (both) and Node<uint64_t>.  Only Node<mpz_class> remains.
* All temporary variables were removed:
  * Use of thread_local creates scratch-pads for temporary mpz/mpf items.
  * Use of mpz_*() methods avoid arithmetic operators which use temp vars.
  * Passing by reference (const T& x), helps.  This is now enforced in for_each() methods with callbacks.
* A bug in coverage was found and fixed (we passed by value, not reference).

#### Tree Building Performance
The tree builds much faster now, as shown in this table

| Version | Data Type | RAM (bytes) | RSS (bytes) | CPU (ms, 1 thr) |
| :------ | :-------- | ----------: | ----------: | --------------: |
| 0.5.1   | uint64_t  |  45,662,928 |         n/a |             245 |
| 0.6.0   | uint64_t  |  85,173,336 |  60,030,976 |             282 |
| 0.8.0   | uint64_t  |  14,681,376 |  14,942,208 |             135 |
| 0.5.1   | mpz_class |  48,808,632 |         n/a |             865 |
| 0.6.0   | mpz_class | 114,115,904 |  66,453,504 |             887 |
| 0.8.0   | mpz_class |  15,730,216 |  18,087,936 |             241 |


#### Allocation Performance Improved
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

### 0.7.0
Refactored the Makefile to be a little better.  We can parallel build now.

Started profiling and we're doing okay with padding, alignment, and so forth, but there's a lot
of waste too with size_t in areas and bools not bit-packed.

Added better CLI support with CLI11.  It's fat, but convenient.

Added a global logger from spdlog.

At this piont I'm not sure better speed is a goal.  Our problems are now in memory, both with size and
allocation.  Resolving these gives more speed and space to work.

### 0.6.1
Reverted to to `std::vector<size_t, std::vector<Node<T>*>>` and threw the SlabAllocator away.

### 0.6.0
Switched from `std::vector<size_t, std::vector<Node<T>*>>` to `std::vector<size_t, std::vector<Node<T>>>` to avoid so many allocations when building the BinaryTree.

#### Goals

* Improve locality by calling `.reserve()` and `.resize()` with `child_count` since it's a deterministic value.
* Reduce allocations by storing Node objects as values instead of pointers to heap.

#### Results
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

#### Conclusion
The loss of parallel construction is unacceptable.  We should either switch back to `Node<T>*` or build a slab allocator.

### 0.5.1
This is the last build before switching to value-based Node storage in BinaryTree.  This is mostly so we can compare performance
changes, namely those related to allocations and tree-building time.

### 0.5.0
Added detection inside of the Collatz class to handle overflow detection.  Runtime performance hit was minimal.

### 0.4.0
Switching away from `std::string` to `std::vector<bool>` for storing the `_oe_pattern`.  Note, the latter is a c++ specialized
template, which makes it act like a bitset (packed bits).

The performance gain was significant.  Memory usage was much lower.  We should probably use a real bitset, but I'm not sure how,
because bitsets generally require compile-time values ... but our sizes are dynamic.

### 0.3.0
BinaryTree now supports parallel construction.  Using OMP offered linear results with CPUs available, so I'm not going to worry
about a custom threader/worker system.  Note, we didn't get linear performance gains building the tree, just steady-state usage of
the CPUs (possible spin-waiting).  However, the gains are on par with parallel technology in general: you rarely get pure linear
performance.

### 0.2.0
First real version of the system in C++.  Using wayyy too much memory for what it's doing.  Python version used roughly 1-2GB, but
this one uses nearly 20GB somehow.  Much of it was tied to mpz_class (GMP), but also bad design decisions when prototyping.  We
need a performance stats program so we can track changes over time.

### 0.1.0
Beginning work on the conversion from Python programs in the tools/ directory to C/C++ in src/.
