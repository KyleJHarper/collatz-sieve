# Things To Do

### Options
* Make a universal options processor class so we can stop writing loops in every program.

### Coverage
* Make this support integral and mpz_class as a runtime decision so we don't have to recompile it when testing.

### Refactor for Cleanliness
* Code is ugly in a lot of areas.  Let's clean that up.

### Tests
* Add a test for Node init() to reuse an object.
* Change static "keep_sequences" to per-node... I guess?  Similar to own_children.
* Do the same with Collatz...?
