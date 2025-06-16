#!/usr/bin/python3
import argparse
from lib.collatz import Collatz

"""
Find stopping pairs of sequential numbers.
"""

# Get args
parser = argparse.ArgumentParser(description="Build a table of integers in decimal, binary, and hex.")
parser.add_argument(
    '--by-group',
    help="Print the values as groups with matching up/down patterns.",
    action='store_true',
    default=False,
    dest='by_group',
)
parser.add_argument(
    '--end',
    type=int,
    help="Ending number.  Default is %(default)s.",
    default=20,
    action='store',
    dest='end',
)
parser.add_argument(
    '--step',
    type=int,
    help="Only calcuate every Nth number (step over the others).  1 means all numbers.  Default is %(default)s.",
    default=1,
    action='store',
    dest='step',
)
parser.add_argument(
    '--start',
    type=int,
    help="Starting number.  Default is %(default)s.",
    default=1,
    action='store',
    dest='start',
)
args = parser.parse_args()
by_group = args.by_group
start = args.start
end = args.end
step = args.step


class UpDownPair:
    @property
    def lower(self):
        return self._lower

    @property
    def upper(self):
        return self._upper

    @property
    def index(self):
        return self._index

    @property
    def stop(self):
        return self.lower.sequence[self._index]

    def __init__(self, lower: Collatz, upper: Collatz, index: int):
        self._lower = lower
        self._upper = upper
        self._index = index

    @classmethod
    def get_key(cls, lower: Collatz, upper: Collatz, index: int) -> tuple:
        return (lower.up_down_string[0:index], upper.up_down_string[0:index])


class UpDownPairManager:
    @property
    def groups(self):
        return self._groups

    @property
    def pair_width(self):
        return self._pair_width

    @property
    def stop_width(self):
        return self._stop_width

    @property
    def value_width(self):
        return self._value_width

    def __init__(self, name: str = ''):
        self._name = name
        self._groups = {}
        self._pair_width = 0
        self._stop_width = 0
        self._value_width = 0

    def add(self, lower: Collatz, upper: Collatz):
        """
        Adds a pair to the group created by the lower and upper.  Errors if it can't.
        """
        index = lower.aligns_at(upper)
        if index < 1:
            raise Warning(f"The lower ({lower.initial_value}) and upper ({upper.initial_value}) don't align.")
            return
        key = UpDownPair.get_key(lower=lower, upper=upper, index=index)
        if key not in self._groups.keys():
            self._groups[key] = []
        self._groups[key].append(UpDownPair(lower=lower, upper=upper, index=index))
        self._pair_width = max(self._pair_width, len(str(lower)) + len(str(upper)))
        self._stop_width = max(self._stop_width, len(str(lower.stopping_time)))
        self._value_width = max(self._value_width, len(str(lower.sequence[index])))


# Do stuff.
matched = set()
prev_sequence = Collatz(start)
match_results = []
space = False
manager = UpDownPairManager()
for i in range(start + 1, end + 1, step):
    sequence = Collatz(i)
    index = sequence.aligns_at(prev_sequence)
    if index > 0:
        if space:
            match_results.append('')
            space = False
        for s in [prev_sequence, sequence]:
            matched.add(s.initial_value)
            match_results.append(f"{s} ({s.stopping_time} stops, ends at {s.sequence[index]}) = {s.up_down_string[0:index]}")
        manager.add(lower=prev_sequence, upper=sequence)
    else:
        space = True
    prev_sequence = sequence


# Print results...
if by_group:
    for group_key, group in manager.groups.items():
        print(f"\nGroup: {group_key}")
        for pair in group:
            pair_key = str(pair.lower) + '/' + str(pair.upper)
            line = f"  Pair: {pair_key:>{manager.pair_width + 2}}"
            line += f"    Stops: {str(pair.lower.stopping_time):>{manager.stop_width}}"
            line += f"    End: {str(pair.lower.sequence[pair.index]):>{manager.value_width}}"
            print(line)
            #print(f"  Pair: {pair.lower}/{pair.upper} -- Stops: {pair.lower.stopping_time} -- Final value: {pair.lower.sequence[pair.index]}")
else:
    for line in match_results:
        print(line)
    unmatched = [str(i) for i in set(range(start + 1, end + 1, step)).difference(matched)]
    print()
    print(f"The following weren't matched: {', '.join(unmatched)}")
