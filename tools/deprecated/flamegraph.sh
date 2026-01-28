#!/usr/bin/bash


# Call perf and flamegraph to get a nice picture.
# We will pass all arguments as-is with "${@}"
if [ -z "${1}" ] ; then
  echo "You have to specify the program and args you want to run." >&2
  exit 1
fi

# The symlink is busted on 24.04.  Use 6.8, per recommendation.
sudo -E perf record -F 99 -g -- "${@}"
sudo chown ${USER}:${USER} perf.data

# Script it.
perf script > out.perf

# Collapse it.
../FlameGraph/stackcollapse-perf.pl out.perf > collapsed.txt

# Build the SVG.
../FlameGraph/flamegraph.pl --width 2000 collapsed.txt > flamegraph.svg


# Clean up.
rm perf.data
rm out.perf
rm collapsed.txt
