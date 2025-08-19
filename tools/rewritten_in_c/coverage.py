#!/usr/bin/env python3

"""
Calculate how much coverage we're getting as we descend the tree.
"""

import argparse
from lib.collatz import BinaryTree
from decimal import Decimal


parser = argparse.ArgumentParser(description="Find the descendant that fulfills the HWM for a node.")
parser.add_argument(
    '--levels',
    type=int,
    help="How many levels in the tree.  Default is %(default)s.",
    default=4,
    action='store',
    dest='levels',
)
args = parser.parse_args()
LEVELS = args.levels


# Build the tree.
tree = BinaryTree(level_count=LEVELS)

# Now calculate coverage.
coverage_map = {}
for level in range(1, LEVELS + 1):
    covered = 0
    for node in tree.levels[level]:
        if node.is_below_high_water_mark or node.has_high_water_mark_ancestor:
            covered += 1
    coverage_map[level] = covered

# Print results.
global_covered = 0
global_total = 0
for level, covered in coverage_map.items():
    total = len(tree.levels[level])
    global_total += total
    global_covered += covered
    coverage = Decimal(100 * covered) / Decimal(total)
    print(f"Level {level}: {coverage:.6f}%  ({covered}/{total})")
# Global
coverage = Decimal(100 * global_covered) / Decimal(global_total)
print(f"Global Coverage: {coverage:.6f}%  ({global_covered}/{global_total})")

print("This should be rewritten in C")
