# Things To Do

### Arbitrary Tree Level
* We should support building a tree from level M-N, where M >=1 and N > M.
* We can build level M manually, which will be expensive.
  * We can build a smaller set of levels (e.g. 1-16) to capture 95+% exclusion to help build M faster.

### Distributed Computing
* The tree should be buildable in an asynchronous manner by multiple systems.
* Requires arbitrary levels, from above.
* Requires an export feature.  Maybe add an import feature to, to save time when testing large trees?

### Refactor for Cleanliness
* Code is ugly in a lot of areas.  Let's clean that up.
