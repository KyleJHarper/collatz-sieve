#!/usr/bin/env python3

max = 50000000000000000
i = 2
step = 0
bit_width = len(str(bin(max))) - 2
while i < max:
    step += 1
    print(f"{step:>2}  " + format(i, f"0{bit_width}b") + f"  {i}")
    i = i * 3 + 2
