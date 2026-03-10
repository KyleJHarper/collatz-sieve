#!/usr/bin/env python3

#
# Find the levels where a contractive exponential factor A is overcome by a constant factor B by evaluating affine maps.
#

#
# Given F and G are:
#   F = 3/2 * N + 0.5
#   G = 1/2 * N
#
# Their affine forms of T(N) = A * N + B are:
#   F: A = 1.5, B = 0.5
#   G: A = 0.5, B = 0
#
# The composition of two affine maps is also affine.  This means only the order of F and G affect the constant portion B.  The
# greatest constant term B that can exist happens when the F steps occur at the end, preventing their contribution from being
# devalued by G steps.
#
# Example for level 6:
#   Chain length will be 5  (L - 1).
#   Largest contractive exponent is 3^3/2^5 ==> 27/32
#     F steps (f): 3
#     G steps (g): 2
#     Total steps (k): 5
#   FFFGG happens on node 55 and creates the following constant term:
#     F: 0 * 1.5 + 0.5 ==> 0.5
#     F: 0.5 * 1.5 + 0.5 ==> 0.75 + 0.5 ==> 1.25
#     F: 1.25 * 1.5 + 0.5 ==> 1.875 + 0.5 ==> 2.375
#     G: 2.375 * 0.5 ==> 1.1875
#     G: 1.1875 * 0.5 ==> 0.59375
#  GGFFF happens on node 60 and creates the following constant term:
#    G: 0 * 0.5 ==> 0
#    G: 0 * 0.5 ==> 0
#    F: 0 * 1.5 + 0.5 ==> 0 + 0.5 ==> 0.5
#    F: 0.5 * 1.5 + 0.5 ==> 0.75 + 0.5 ==> 1.25
#    F: 1.25 * 1.5 + 0.5 ==> 1.875 + 0.5 ==> 2.375
#
# Since the contractive exponent is 27/32, the maximal constant term B (2.375) cannot overcome the contractive effect.  We can
# extend this to theoretical limits by generating worst-case scenarios:
#   Assume some 3^f/2^k yields (2^k - 1)/2^k.
#     Rephrased, assume X = 2^k (the denominator).  There exists numerators of (X - 1), and therefore factors like (X - 1)/X.
#     Examples include 1/2 and 3/4, and theoretically infinitely more.
#   The smallest remaining factor of N to overcome is: 1 - (contractive factor).
#     Examples: 1 - 1/2 ==> 1/2.   1 - 3/4 ==> 1/4.
#   Since the denominator is the determining factor, there exists a limit on any level L of 1/(2^k) * N for B to overcome.
#   Values of N at any level are fixed, and align with our denominator because of the same dyadic scaling:
#     Minimum N = 2^(L-1)
#     Maximum N = 2^L - 1
#   These values align with the largest contractive factor above to create the following requirement of B to overcome contraction:
#     1 <= B < 2
#
# Since B can grow with additional F steps, and the FG chain grows per level, but the requirement to overcome contraction is fixed,
# there may exist levels where the contractive factor is overcome by the constant portion to reach >= N, creating:
#   Potential loops (landing back on N; see the 1-4-2 sequence)
#   Cases where the High-Water mark fails validation purely by the exponential contractive factor alone
#
# This program will loop through many levels, calculate the maximal contractive factor A and constant term B and test for cases
# where the constant term results in >= N.  At a minimum, this program will help us set limits on the CollatzAffineMapShortcut
# class, allowing us to leverage its performance gain without creating false High-Water Mark nodes.
#
# Since we only want to overcome contraction (>= N), we will only look at the shortest distance, which is always a theoretical 1
# realistically:
#   (denominator - numerator) / denominator * smallest_N
#   ==> (denominator - numerator)   [because denominator and smallest_N are always the same value]
#
# If you want to see the theoretical maximum for B grow closer to 2 without touching it, set --maximums.
#
# WHEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE!!!!!

import argparse
from decimal import Decimal, getcontext
import sys
import multiprocessing


DEFAULT_MAX_PRECISION = 1000
parser = argparse.ArgumentParser(description="Find the levels where a contractive exponential factor A is overcome by a constant factor B by evaluating affine maps.")
parser.add_argument(
    '--levels',
    type=int,
    help="The number of levels to test to.",
    action='store',
    default=10,
)
parser.add_argument(
    '--max-precision',
    type=int,
    help="Max precision to use for floats (Decimal type).  Program will auto-scale to it.  Zero == full  (aka: level count)",
    action='store',
    default=DEFAULT_MAX_PRECISION,
)
parser.add_argument(
    '--verbose',
    help="Enable verbosity.",
    action='store_true',
    default=False,
)
args = parser.parse_args()
LEVELS = args.levels
VERBOSE = args.verbose
MAX_PRECISION = args.max_precision
if MAX_PRECISION == 0:
    MAX_PRECISION = LEVELS

if LEVELS < 2:
    print("You must specify a level (--levels) greater than 1", file=sys.stderr)
    exit(1)

full_precision = LEVELS
final_precision = full_precision
if final_precision > MAX_PRECISION:
    print(f"The required precision {final_precision} exceeds the max of {MAX_PRECISION} and will be reduced.  Adjust with --max-precision if desired.", file=sys.stderr)
    final_precision = MAX_PRECISION
getcontext().prec = final_precision
if VERBOSE:
    print(f"Precision set to {final_precision}")


#
# Helper for multiprocessing.
# Returns <level> if B overcomes contractive A to reach >= N, otherwise 0.
#
def do_it(level: int):
    total_steps = level - 1
    denominator = Decimal('2') ** Decimal(total_steps)
    f_steps = 0
    while 3 ** (f_steps + 1) <= denominator:
        f_steps += 1
    numerator = 3 ** f_steps
    min_N = (Decimal('2') ** (level - 1))
    max_N = (Decimal('2') ** level) - 1
    max_B = Decimal('0')
    for _ in range(1, f_steps + 1):
        max_B = max_B * Decimal('3') / Decimal('2')
        max_B += Decimal('0.5')
    theoretical_min_distance_to_N = min_N / denominator
    theoretical_max_distance_to_N = max_N / denominator
    true_min_distance_to_N = min_N * (denominator - numerator) / denominator
    true_max_distance_to_N = max_N * (denominator - numerator) / denominator
    if VERBOSE:
        print(
            f"Level {level}:"
            f", min_N={min_N}, max_N={max_N} f_steps={f_steps}, total_steps={total_steps}"
            f", A={numerator}/{denominator}, max_B={max_B}"
            f", theoretical_min_distance_to_N={theoretical_min_distance_to_N}, theoretical_max_distance_to_N={theoretical_max_distance_to_N}"
            f", true_min_distance_to_N={true_min_distance_to_N}, true_max_distance_to_N={true_max_distance_to_N}"
        )
    if max_B >= true_min_distance_to_N:
        print(f"Level {level} can exceed N with A = {numerator}/{denominator} and max B = {max_B}")
        return level
    return 0


#
# Main
#
with multiprocessing.Pool() as pool:
    results = pool.map(do_it, range(2, LEVELS + 1))

met_or_exceeded = [result for result in results if result > 0]
print(f"There were {len(met_or_exceeded)} which met or exceeded N.  They are: {met_or_exceeded}")
