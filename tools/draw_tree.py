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
from lib.collatz import BinaryTree


parser = argparse.ArgumentParser(description="Build an expanding window graph.")
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
