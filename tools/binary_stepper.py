#!/usr/bin/env python3

import argparse
from lib.collatz import Collatz
from typing import List


parser = argparse.ArgumentParser(description="Step through the binary representation of a number and puts it into a columnar layout.")
parser.add_argument(
    'initial_values',
    type=int,
    nargs='*',
    help="The number(s) to test.  If multiple, we'll put them side-by-side.",
    action='store',
)
parser.add_argument(
    '--diff',
    help="Enables differential view of current value to previous value in each step.",
    action='store_true',
    dest='diff',
    default=False,
)
parser.add_argument(
    '--diff-spacer',
    type=str,
    help="The character to use when --diff is enabled for matching chars.  Default is '%(default)s'.",
    action='store',
    dest='diff_spacer',
    default='_',
)
args = parser.parse_args()
initial_values = args.initial_values
diff = args.diff
diff_spacer = args.diff_spacer


sequences: List[Collatz] = []
for initial_value in initial_values:
    sequences.append(Collatz(initial_value=initial_value))
bit_widths = {}
decimal_widths = {}
field_spacers = []
max_stops = max(map(lambda sequence: sequence.stop_count, sequences))
padding = 2
for sequence in sequences:
    bit_widths[sequence] = max(len(str(bin(max(sequence.sequence)))) - 2, 8)
    decimal_widths[sequence] = len(str(max(sequence.sequence)))
    field_spacers.append(' ' * (bit_widths[sequence] + decimal_widths[sequence] + padding))
i = 0
while i < max_stops:
    results = []
    diff_results = []
    for sequence in sequences:
        # We need bit and decimal widths for spacing correctly.
        bit_width = bit_widths[sequence]
        decimal_width = decimal_widths[sequence]
        # Now process the sequence's current stop.
        previous_stop = None
        if i >= sequence.stop_count:
            results.append(" " * ((bit_width + decimal_width) + padding))
        else:
            stop = sequence.sequence[i]
            if i > 0:
                previous_stop = sequence.sequence[i-1]
            binary = format(stop, f"0{bit_width}b")
            results.append(f"{binary:>{bit_width}}{' ' * padding}{stop:>{decimal_width}}")
        # If we're diff-ing, get that value too.
        if diff:
            if i >= sequence.stop_count or previous_stop is None:
                diff_results.append(" " * ((bit_width + decimal_width) + padding))
            else:
                # We should have 'binary' from above.  Make a previous binary and compare characters.
                previous_binary = format(previous_stop, f"0{bit_width}b")
                binary_diff = ''
                for b_index in range(0, len(binary)):
                    binary_diff += diff_spacer if binary[b_index] == previous_binary[b_index] else binary[b_index]
                diff_results.append(f"{binary_diff:>{bit_width}}{' ' * padding}{' ' * decimal_width}")
    print('  |  '.join(results))
    if diff:
        print('  |  '.join(diff_results))
        print('  |  '.join(field_spacers))
    i += 1
