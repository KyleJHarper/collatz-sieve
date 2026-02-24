#!/usr/bin/env python3

"""
Try to model the expanding window so it's less error-prone from human math and display as text.
Like so:

L1 (+0)                                             1
                                                    |
L2 (+1)                      2______________________|____________________3
                             |                                           |
L3 (+2)           4__________|__________6                     5__________|__________7
                  |                     |                     |                     |
L4 (+4)      8____|_____12        10____|_____14         9____|_____13        11____|_____15
             |          |          |          |          |          |          |          |
L5 (+8)  16__|__24  20__|__28  18__|__26  22__|__30  17__|__25  21__|__29  19__|__27  23__|__31
"""

import argparse
from lib.collatz import BinaryTree


parser = argparse.ArgumentParser(description="Build a tree and print it into an SVG that's easy to read/visualize.")
parser.add_argument(
    '--levels',
    type=int,
    help="Number of levels deep to go.  Default is %(default)s.",
    default=4,
    action='store',
    dest='levels',
)
args = parser.parse_args()
MAX_LEVELS = args.levels


# Build the tree.
tree = BinaryTree(level_count=MAX_LEVELS)
tree.render('zz_sample.svg')
