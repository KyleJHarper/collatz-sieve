# Things To Do

We have two major goals, and I've slid into disarray and scope creep, so we'll keep it straight here.


## Goal 1: Sieve

I want to build a sieve that exceeds anything previously built.  Here are the guidelines:

* Is fully deterministic and complete: no probability.
* Optimizes the problem space (higher rejection rate).  Don't care about faster hardware or software.
* Results in a new algorithm for space testing that is employable in a reasonable manner (easy iteration through uncovered nodes).


## Goal 2: Solution

Pride isn't the goal: this problem might not be solvable.  However, I coined a term in my career called "optimization toward zero".
This technique takes a problem and attempts to resolve it by eliminating it.  While most often used by removing steps in a chain or
pipeline which had issues (thereby optimizing the problem away), it might apply here.

As such, I'll leave this as a noble, albeit silly, goal.  Simply put:

* Create a sieve with such coverage the space is completely tested (sieve == 100% exclusion).

## Current Work

### Update Tree Generator
The draw_tree.py script works, but only for 0-based tree.  We should probably rewrite the whole thing (Tree/Node classes) to follow
our BinaryTreeMath logic in C.

### Scan for Ancestor Resolution
* We know that descendents end up with a sequence which solves an ancestor.  Can we find a proof for that?

### HWM Ancestor Filter
```
for each number N:
  if some_func(N) proves N is a descendant of any_ancestor
    skip it
```

^ This would mean precomputation of HWM ancestors and the coverage they create can now be applied
without building a whole tree, or even the the last level of a tree.

^ This would also mean you could build any arbitrary level, and optionally scan for HWM ancestors in
it manually.

### Sieve Basics
We need a for_each or next() style method for fetching survivors from our tree, giving us a sieve.

### Arbitrary Tree Level
* We should support building a tree from level M-N, where M >=1 and N > M.
* We can build level M manually, which will be expensive.
  * We can build a smaller set of levels (e.g. 1-16) to capture 95+% exclusion to help build M faster.

### Distributed Computing
* The tree should be buildable in an asynchronous manner by multiple systems.
* Requires arbitrary levels, from above.

I believe we can track HWM ancestors and put them in a table.  A future node can determine if it is a
descendant of a HWM ancestor by using the level the ancestor is on as the "step" value and the difference
is the sum of the step-values from the ancestors level to the current node level.

This would allow the efficient building of a higher level in the tree without needing the entire tree itself.
Once level M is built, the tree can continue building to level N.

Ex:
```
       5 (hwm ancestor)
      / \
     /   \
    /     \
   9       13
  / \      / \
 /   \    /   \
17   25  21   29

Given 5 is a HWM Ancestor, test 9:
5's Level is 2, 5's child step is 4
9's Level is 3, 9's child step is 8
for (i=0; i<(3-2); i++)
  if N(9) - (CHILD_STEP(4) * 2^i) == HWM_ANCESTOR(5)
    return FOUND_IT(true)

Iteration 1 (i == 0):
  if 9 - (4 * 2^0) == 5
  ==> if 9 - (4 * 1) == 5
  ==> if 9 - (4) == 5
  ==> found it.

Now for 13 (right-hand child)
5's level is 2, 5's child step is 4
13's level is 3, 13's child step is 8
(Same Logic from Above)

Iteration 1 (i == 0):
  if 13 - (4 * 2^0) == 5
  ==> if 13 - (4 * 1) == 5
  ==> if 13 - (4) == 5
  ==> false.
Iteration 2 (i == 1):
  if 13 - (4 * 2^1) == 5
  ==> if 13 - (4 * 2) == 5
  ==> if 13 - (8) == 5
  ==> found it.

17, 25, 21, and 29 are children of HWM(5) too.  Test those.
N-Level is 4 now.  HWM-Level is still 2.  CHILD_STEP is 8.
for (i=0; i<(4-2); i++)
  if N(17) - (CHILD_STEP(8) * 2^i) == HWM(5)
    return FOUND_IT(true)

Iteration 1 (i == 0)
  if 17 - (8 * 2^0) == 5
  ==> if 17 - (8 * 1) == 5
  ==> if 17 - (8) == 5
  ==> false.
Iteration 2 ( i == 1)
  if 17 - (8 * 2^1) == 5
  ==> 17 - (8 * 2) == 5
  ==> 17 - (16) == 5

^ Here's the breakdown.  We need to continue stepping 4... HWM_CHILD_STEP
Here is the iterations again, but with 4:
Iteration 1 (i == 0)
  if 17 - (4 * 2^0) == 5
  ==> if 17 - (4 * 1) == 5
  ==> if 17 - (4) == 5
  ==> false.
Iteration 2 (i == 1)
  if 17 - (4 * 2^1) == 5
  ==> if 17 - (4 * 2) == 5
  ==> if 17 - (8) == 5
  ==> false.
Iteration 3 (i == 2)
  if 17 - (4 * 2^2) == 5
  ==> if 17 - (4 * 3) == 5
  ==> if 17 - (12) == 5
  ==> FOUND_IT(true)

However, this is still wrong.  It won't pick up 25 or 29.
I think we need to use both 4 and 8, with series summation.
Or maybe just offset by the level difference ...:
for (i=0; i<(4-2); i++)
  if N(17) - (N_CHILD_STEP(8)) (HWM_CHILD_STEP(4) * 2^i) == HWM(5)
    return FOUND_IT(true)
Iteration 1 (i == 0)
  if 17 - 8 - (4 * 2^0) == 5
  ==> if 17 - 8 - (4 * 1) == 5
  ==> if 17 - 8 - (4) == 5
  ==> FOUND_IT(true)

Now for 21
Iteration 1 (i == 0)
  if 21 - 8 - (4 * 2^0) == 5
  ==> if 21 - 8 - (4 * 1) == 5
  ==> if 21 - 8 - (4) == 5
  ==> false.
Iteration 2 (i == 1)
  if 21 - 8 - (4 * 2^1) == 5
  ==> if 21 - 8 - (4 * 2) == 5
  ==> if 21 - 8 - (8) == 5
  ==> FOUND_IT(true)

```
