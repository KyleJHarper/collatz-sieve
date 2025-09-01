#!/bin/bash

set -e

for build in 'Debug' 'Release' ; do
  bin/${build}/test__collatz_class
  bin/${build}/test__node_class
  bin/${build}/test__binary_tree_class
  bin/${build}/test__sieve_class
  bin/${build}/test__forward_looking_cache_class
done

echo ""
echo "All Good"

