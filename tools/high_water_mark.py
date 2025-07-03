#!/usr/bin/env python3

import argparse
import sys

# We use big numbers ... let them be printed.
sys.set_int_max_str_digits(10000)

parser = argparse.ArgumentParser(description="No idea yet...")
parser.add_argument(
    '--max-exp',
    type=int,
    default=10,
    help="The highest exponent to test to.  Default is: %(default)s.",
    action='store',
    dest='max_exp'
)
parser.add_argument(
    '--show-fg',
    help="Show f(x) and g(x) patterns required to hit high-water mark.",
    action='store_true',
    dest='show_fg'
)
args = parser.parse_args()
max_exp = args.max_exp
show_fg = args.show_fg


exp_2 = 0
exp_3 = 0
while exp_3 < max_exp:
    # Increase numerator (3) until the next increase would tip it over 1 again.
    while pow(3, exp_3) < pow(2, exp_2):
        exp_3 += 1
    # Increase denominator (2) until we get under 1.
    while pow(3, exp_3) >= pow(2, exp_2):
        exp_2 += 1
    # We have a 2^x/3^y match now.
    print(f"{pow(3, exp_3)/pow(2, exp_2)},  3^{exp_3} / 2^{exp_2}, {pow(3, exp_3)}/{pow(2, exp_2)}")
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
