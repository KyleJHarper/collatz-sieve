# Current Work

## Re-verify GPU on Peak by Bit
For my own sanity, re-run the peak by bit program up to 80-90 bits and make sure I didn't introduce a bug that's preventing us from
finding 2^109 lately.

## Map Remainder of Peak by Bit
We are stuck at 2^109 for peak-by-bit.  I burned an RTX-5060 at 100% for 4 months getting to that point.  Not sure how we'll get
those last 19 places...

## Sieve
* Flesh this out so it's ironclad.
* Fully implemented the forward-looking cache.
* Switch to an implicit tree.
* Leverage a GPU, if possible.

## Scan for Ancestor Resolution
* We know that descendents end up with a sequence which solves an ancestor.  Can we find a pattern that describes it?
