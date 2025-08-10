# Overview

### Branching

Historically, all work was done in "master".  We will use "dev" for all development now.  We can feature branch if needed later.

### Tags

I prefer SemVer, but given the research nature of this project, I'm not sure it makes sense.  As such, we will use 0.x.y until a
better format avails itself.

I will tag things as new features are developed and list those features in the change log below.

# Change Log

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

| Version | Data Type | Allocations | For GMP | Temp |
| :------ | :-------- | ----------: | ------: | ---: |
| 0.5.1 | Node<uint64_t>* | 1,462,735 | 786,477 | ?? |
| 0.6.0 | Node<uint64_t> | 2,158,447 | 1,441,832 | ?? |
| 0.8.0 | Node<uint64_t>* | 79 | 47 | 17 |
| 0.5.1 | Node<mpz_class>* | 7,566,388 | 6,759,076 | ?? |
| 0.6.0 | Node<mpz_class> | 8,393,170 | 7,545,501 | ?? |
| 0.8.0 | Node<mpz_class>* | 665,703 | 655,636 | 131,138 |

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

| Version | Data Type | Allocations | For GMP |
| :------ | :-------- | ----------: | ------: |
| 0.5.1 | Node<uint64_t>* | 1,462,735 | 786,477 |
| 0.6.0 | Node<uint64_t> | 2,158,447 | 1,441,832 |
| 0.5.1 | Node<mpz_class>* | 7,566,388 | 6,759,076 |
| 0.6.0 | Node<mpz_class> | 8,393,170 | 7,545,501 |

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
