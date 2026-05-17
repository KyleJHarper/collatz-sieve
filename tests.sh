#!/bin/bash

set -e

for build in 'Debug' 'Release' ; do
  for program in bin/${build}/test__* ; do
    ${program}
    echo
  done
done

echo ""
echo "All Good"

