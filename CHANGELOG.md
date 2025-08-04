# Overview

### Branching

Historically, all work was done in "master".  We will use "dev" for all development now.  We can feature branch if needed later.

### Tags

I prefer SemVer, but given the research nature of this project, I'm not sure it makes sense.  As such, we will use 0.x.y until a
better format avails itself.

I will tag things as new features are developed and list those features in the change log below.

# Change Log

### 0.6.0
Switched from `std::vector<size_t, std::vector<Node<T>*>>` to `std::vector<size_t, std::vector<Node<T>>>` to avoid so many
allocations when building the BinaryTree.  Goals:

* Improve locality by calling `.reserve()` and `.resize()` with `child_count` since it's a deterministic value.
* Reduce allocations by storing Node objects as values instead of pointers to heap.
* TODO: Results???

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
