#!/usr/bin/env python3

import math

b = 0
for length in range(1, 64, 1):
    b = ((b * 3) + 1) / 2
    print(f"{length})  b: {b}  log2: {math.log2(b)}   3_exp: {3**length}   3_bits: {math.log2(3**length)}")


print(f"Final b is {b} and its log2 is {math.log2(b)}")

