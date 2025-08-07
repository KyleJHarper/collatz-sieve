# Things To Do

### Performance Tuning
* Optimize Collatz and Node classes as lean as possible.
  * Pack bools as bits.
  * Switch from size_t to uint8_t for items like _child_count.
  * Remove unnecessary members, if any, or decide if they can be calculated in getters realtime.
* Revisit slab allocation and a Pool.
  * When the tree begins to self prune, this will become critical.
  * With so much of our time spent in GMP init/alloc, I wonder if a global pool can help.
  * We're still off by nearly 75% on our deep_size() vs RSS reporting on uint64_t.

### Refactor for Cleanliness
* Code is ugly in a lot of areas.  Let's clean that up.
