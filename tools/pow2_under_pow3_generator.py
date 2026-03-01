#!/usr/bin/env python3

exp_3 = 0
exp_2 = 0
MAX_3 = 512
results = []

while exp_3 < MAX_3:
    threes_value = 3**exp_3
    twos_value = 2**exp_2
    while threes_value > twos_value:
        exp_2 += 1
        twos_value = 2**exp_2
    # Should have an exp_2 that's over 3.  So it's -1 to get the highest.
    exp_2 -= 1
    twos_value = 2**exp_2
    print(f",{exp_2}   // 3^{exp_3} > 2^{exp_2} ==> {threes_value:,} > {twos_value:,}  (next would be {2**(exp_2 + 1):,})")
    results.append(exp_2)
    exp_3 += 1

# Print results for array.
#for exp_3, exp_2 in  enumerate(results):
#    print(f"{exp_2},  // 3^{exp_3} > 2^{exp_2}")

