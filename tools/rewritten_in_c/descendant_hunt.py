#!/usr/bin/env python3

"""
Try to model the expanding window so it's less error-prone from human math and display as text.
Like so:

L0 (+0)                                             0
                                                    |
L1 (+1)                      1______________________|____________________2
                             |                                           |
L2 (+2)           3__________|__________5                     4__________|__________6
                  |                     |                     |                     |
L3 (+4)      7____|_____11         9____|_____13         8____|_____12        10____|_____14
             |          |          |          |          |          |          |          |
L4 (+8)  15__|__23  19__|__27  17__|__25  21__|__29  16__|__24  20__|__28  18__|__26  22__|__30
"""

import argparse
from lib.collatz import BinaryTree, Collatz


parser = argparse.ArgumentParser(description="Find the descendant that fulfills the HWM for a node.")
parser.add_argument(
    '--levels',
    type=int,
    help="How many levels of nodes to find descendants for.  Default is %(default)s.",
    default=4,
    action='store',
    dest='levels',
)
parser.add_argument(
    '--max-levels',
    type=int,
    help="How many levels can the tree grow to find a descendant before giving up.  Default is %(default)s.",
    default=10,
    action='store',
    dest='max_levels',
)
args = parser.parse_args()
LEVELS = args.levels
MAX_LEVELS = args.max_levels


# Build the tree.
tree = BinaryTree(level_count=MAX_LEVELS)

# Find all the nodes who are below HWM and the descendant which fulfills them.

for level in range(0, LEVELS + 1):
    if level == 0:
        continue
    nodes = tree.levels[level]
    for node in nodes:
        if node.is_below_high_water_mark or node.has_high_water_mark_ancestor:
            continue
        if node.value == 1:
            print("Node is 1. Skipping")
            continue
        sequence = Collatz(node.value)
        pattern_needed = Collatz.generate_oe_pattern(sequence.high_water_mark_sequence)
        print(f"Node {node} doesn't meet HWM.  Need {pattern_needed}")
        descendant = node
        while descendant.oe_chain != pattern_needed:
            if len(descendant.children) == 0:
                print(f"No more children after descendant {descendant}.  Cannot make tree bigger.")
                break
            for child in descendant.children:
                if pattern_needed.startswith(child.oe_chain):
                    descendant = child
                    break
        if descendant.oe_chain == pattern_needed:
            print(f"  > Found node {descendant} to be the match for {node}.")
