#!/usr/bin/python3

import argparse
from lib.collatz import Collatz


parser = argparse.ArgumentParser(description="Analyze the e7 element in my Collatz window16 view by digging into the depth of its cycle and repetitions.")
parser.add_argument(
    '--max-levels',
    type=int,
    help="How many levels deep to go.  Default is %(default)s.",
    default=5,
    action='store',
    dest='max_levels',
)
parser.add_argument(
    '--oe-repeat-size',
    type=int,
    help="How many times Odd/Evens must repeat to be empirically continuous.  Default is %(default)s.",
    default=100,
    action='store',
    dest='oe_repeat_size',
)
args = parser.parse_args()
max_levels = args.max_levels
oe_repeat_size = args.oe_repeat_size


#
# Main
#
root_sequence = Collatz(7)
prefix = root_sequence.find_oe_prefix(increase=32)
print(f"The prefix is {len(prefix)} characters long.  Odd-Even repeating starts at position {len(prefix) + 1}.")
