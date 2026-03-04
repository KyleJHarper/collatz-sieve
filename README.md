# Overview

An implementation of the data structure outlined in Kyle Harper's analysis and optimization of the Collatz Conjecture's problem
space.  It builds a Binary Tree in a unique, deterministic manner which allows the classification of parents and children into
__F-G Chains__.  These chains create _coverage_ of __High-Water Mark__ nodes and therefore subtrees.  The result eliminates large
subtrees, leaving less than 1% of the problem space N after 33 levels.  Such a tree can be built in a few seconds on a desktop CPU,
requiring only 250MB RAM.

## An Aside

This project started as a curiosity years ago.  I never intended to take the programming portion as far as I did, nor write an API
for building the data structure supporting my hypothesis.

Yet here we are...

## Present State

The project is semi-active.  I have no intention to add more features, but I will fix bugs, if any.  I will also consider merge
requests, especiall in these areas:

* Bug fix.
* Non-breaking optimization.
* Save/load features.
* Build and portability updates, especially for non-linux.

The following features should work:

* The `BinaryTree` interface, including both `BinaryTreeMaterialized` and `BinaryTreeImplicit`.
* The `Collatz` class and its features.
* The `Node` class and its features.
* The `BinaryTreeMath` and logic it provides, tied to the data structure of Harper's technique.

The following progams have these statuses:

| Program               | Status     | Description |
| :-------------------- | :--------- | :---------- |
| `collatz_compression` | Working    | Demonstrates some of the optimizations when testing sequences. |
| `coverage`            | Working    | Builds a tree level by level and reports info about coverage. |
| `junk`                | Special    | Debugging cpp for quick one-off testing.  You can ignore it. |
| `peak_by_bit`         | Working    | Finds overflows in Collatz sequences for `2^bit` size.  GPU augment tested on an RTX-5060. |
| `performance_stats`   | Working    | Debugging tool for analyzing speed and memory performance on a system. |
| `single_collatz`      | Working    | Builds a single sequence and emits some stats about it.  Very simple |
| `sieve_stuff`         | Unfinished | Tool to measure effects of an LRU to aid in generic sieve. |
| `step_counter`        | Unfinished | Tool to analyze steps and organize them. |

## Building

The build system is CMake and should be semi-easy to use.  The `rebuild.sh` script helps clone repos, link things, and execute
`cmake` to build and link programs.

If you're a build expert and want to make an MR to make this smoother for others, please do.  Thanks.

Once built, you can execute `tests.sh` to run all the unit test programs.  These serve as regression testing programs too.

Builds with both `gcc` and `clang` work and pass all tests.

## Performance Comparisons

I came to reduce space complexity, nothing more.  But comparison to others' works isn't apples-to-apples.

Let's discuss...

Many others have built programs.  Some leverage CPUs, GPU, and even distributed workloads across supercomputers.  Chief among these
is the project led by [David Barina](https://link.springer.com/article/10.1007/s11227-025-07337-0).  His paper addresses most of
the history and performance markers leading up to his work in achieving 2^71 sequential testing.  You should read it.

In his work, Barina et al. leverages thousands of CPUs and GPUs across supercomputers in a distributed workload.  He graciously
includes details of the volume of time, and it's staggering: 12,395 CPU-years and 159 GPU-years.

The problem with these types of numbers is they are hardware dependant.  We could build an ASIC to test faster, but we haven't
reduced the problem space.  Both Barina's, my, and others' techniques address both space complexity (test fewer numbers by
convention) and time complexity (test unskipped numbers faster).  Distinguishing between which ones provide overall performance
gains can be tricky.  So instead, I will simply put some basic values here, and (try to) remember to link to my final research
paper with more details later.

### Space Complexity

The nature of the `BinaryTree` reduces space complexity.  It is fully deterministic and therefore a simple metric to calculate and
emit.  The following table was built using an Intel i5-14600k (12 cores enabled) with DDR-6000.  It is a desktop PC.

| Tree Levels | Coverage | Time (sec) |
| ----------: | -------: | ---------: |
|          10 |   92.58% |         <1 |
|          20 |   97.14% |         <1 |
|          30 |   98.81% |         <1 |
|          35 |   99.12% |          2 |
|          40 |   99.34% |        120 |

### Time Complexity

Time complexity involves all manner of optimizations (code) and will be available when I get the `Sieve` class finished.

TODO
