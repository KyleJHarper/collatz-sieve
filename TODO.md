# Things To Do

We have two major goals, and I've slid into disarray and scope creep, so we'll keep it straight here.


## Goal 1: Sieve

I want to build a sieve that exceeds anything previously built.  Here are the guidelines:

* Is fully deterministic and complete: no probability.
* Optimizes the problem space (higher rejection rate).  Don't care about faster hardware or software.
* Results in a new algorithm for space testing that is employable in a reasonable manner (easy iteration through uncovered nodes).
* Exposes new facts which might help future analysts, such as the peak_by_bit program.

## Goal 2: Solution

Pride isn't the goal: this problem might not be solvable.  However, I coined a term in my career called "optimization toward zero".
This technique takes a problem and attempts to resolve it by eliminating it.  While most often used by removing steps in a chain or
pipeline which had issues (thereby optimizing the problem away), it might apply here.

As such, I'll leave this as a noble, albeit silly, goal.  Simply put:

* Create a sieve with such coverage the space is completely tested (sieve == 100% exclusion).

## Current Work

### Re-verify GPU on Peak by Bit
For my own sanity, re-run the peak by bit program up to 80-90 bits and make sure I didn't introduce a bug that's preventing us from
finding 2^109 lately.

### Map Remainder of Peak by Bit
We are stuck at 2^109 for peak-by-bit.  I burned an RTX-5060 at 100% for 4 months getting to that point.  Not sure how we'll get
those last 19 places...

### Bypass Affine Map's calculate()
I believe it's mathematically impossible for an exponential portion of FG to have a 3^x/2^y < 1 where the constant portion brings
it over 1 and to a whole number.  This would mean `is_below()` could simply check exponents as integers (lookup table) and do an
integer comparison.

### Save & Load Tree
Both tree types are fully deterministic.  There's no reason we can't build an export/import feature to save and load a tree.  This
would allow precomputing very large trees and sharing them.

### Sieve
* Flesh this out so it's ironclad.
* Fully implemented the forward-looking cache.
* Switch to an implicit tree.

### Scan for Ancestor Resolution
* We know that descendents end up with a sequence which solves an ancestor.  Can we find a proof for that?
