#!/usr/bin/env python3

import argparse
from datetime import datetime
import sys
import math


parser = argparse.ArgumentParser(description="No idea yet...")
parser.add_argument(
    '--end',
    type=int,
    default=20,
    help="The exponent of 3 to end with, inclusive.  Default is: %(default)s.",
    action='store',
    dest='end'
)
parser.add_argument(
    '--expanded',
    help="Expand the results of 3^x/2^y into a fraction (creates 3rd column).",
    action='store_true',
    dest='expand'
)
parser.add_argument(
    '--show-fg',
    help="Show f(x) and g(x) patterns required to hit high-water mark.",
    action='store_true',
    dest='show_fg'
)
parser.add_argument(
    '--precision',
    type=int,
    default=14,
    help="How much precision to show for the float value.  Useful limit is ~15.  Default is: %(default)s.",
    action='store',
    dest='precision'
)
parser.add_argument(
    '--progress',
    type=int,
    default=0,
    help="How often to show progress in whole percent (1-100).  Default is: %(default)s (0 == disabled).",
    action='store',
    dest='progress'
)
parser.add_argument(
    '--start',
    type=int,
    default=0,
    help="The exponent of 3 to start with, inclusive.  Default is: %(default)s.",
    action='store',
    dest='start'
)
args = parser.parse_args()
expand = args.expand
show_fg = args.show_fg
precision = args.precision
progress_percent = args.progress / 100
start = args.start
end = args.end


exp_2 = 0
total = end - start
next_progress_report = progress_percent
for exp_3 in range(start, end + 1):
    # Progress report.
    done = exp_3 - start
    if progress_percent > 0 and next_progress_report < (done / total):
        print(f"Finished {100 * done / total}% ({done}/{total}) at {datetime.now()}", file=sys.stderr)
        next_progress_report += progress_percent
    # Get the integer value of the current power of 3.
    int_3 = pow(3, exp_3)
    # The power of two will simply be the log-base2 of the integer, ceiling.
    exp_2 = math.floor(math.log(int_3, 2)) + 1
    # We have a 2^x/3^y match now.
    value = pow(3, exp_3) / pow(2, exp_2)
    line = f"3^{exp_3} / 2^{exp_2},  {value:0<.{precision}f}"
    if expand:
        line += f",  {pow(3, exp_3)} / {pow(2, exp_2)}"
    print(line)
    # Show pattern.
    if show_fg:
        pattern = []
        f_done = 0
        g_done = 0
        f_count = exp_3
        g_count = exp_2 - exp_3
        peak_constant = 0
        while f_done < f_count or g_done < g_count:
            # If the next power of 2 would hit HWM, bump 3, but only if we're allowed to.
            if pow(3, f_done) < pow(2, f_done + g_done + 1) and f_done < f_count:
                pattern.append('f')
                f_done += 1
                peak_constant = (peak_constant * 3 / 2) + 0.5
            else:
                pattern.append('g')
                g_done += 1
                peak_constant = peak_constant / 2
        print("  " + '->'.join(pattern))
        print(f"  Peak constant: {peak_constant}")
