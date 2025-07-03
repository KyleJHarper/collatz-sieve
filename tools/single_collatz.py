#!/usr/bin/python3
import sys
from lib.collatz import Collatz


sequence = Collatz(int(sys.argv[1]))
print(', '.join([str(s) for s in sequence.sequence]))
print(f"Its step count (stopping time) is: {sequence.stopping_time}")
print(f"OE Pattern: {sequence.oe_pattern}  ({len(sequence.oe_pattern)})")
print(f"FG Pattern: {sequence.fg_pattern}  ({len(sequence.fg_pattern)})")
