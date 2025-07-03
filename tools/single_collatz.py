#!/usr/bin/python3
import sys
from lib.collatz import Collatz


sequence = Collatz(int(sys.argv[1]), step=16)
print(', '.join([str(s) for s in sequence.sequence]))
print(f"It's step count (stopping time) is: {sequence.stopping_time}")
