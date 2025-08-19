#!/usr/bin/env python3
import sys
from lib.collatz import Collatz


sequence = Collatz(int(sys.argv[1]))
print(', '.join([str(s) for s in sequence.sequence]))
print(f"Its step count (stopping time) is: {sequence.stopping_time}")
print(f"OE Pattern: {sequence.oe_pattern}  ({len(sequence.oe_pattern)})")
print(f"FG Pattern: {sequence.fg_pattern}  ({len(sequence.fg_pattern)})")
print(f"High water mark index is {sequence.high_water_mark_index}, which is the value {sequence.high_water_mark_value}")
print(f"Its fg pattern at hwm is: {Collatz.generate_fg_pattern(sequence.high_water_mark_sequence)}")
x = int(sys.argv[2])
oe = sequence.oe_pattern[0: x]
p_three = pow(3, oe.count('O'))
p_two = pow(2, oe.count('E'))
print(f"Temp: {sequence.oe_pattern[0: x]}")
print(f"Exp: {p_three}/{p_two}")
