#!/usr/bin/env python3

import argparse

parser = argparse.ArgumentParser(description="Analyze the levels of our expanding window.")
parser.add_argument(
    'max_level',
    type=int,
    help="The level to end at.",
    action='store',
)
parser.add_argument(
    '--diffs',
    type=int,
    default=1,
    help="How many levels deep to take the differentials of each level.  Default is %(default)s.",
    action='store',
    dest="diffs",
)
parser.add_argument(
    '--element-spacing',
    type=int,
    default=3,
    help="How much to space elements when printing them.  Default is %(default)s.",
    action='store',
    dest="element_spacing",
)
parser.add_argument(
    '--sub-series',
    type=int,
    default=2,
    help="Break each series in sub-series every X terms. Default is %(default)s.",
    action='store',
    dest="sub_series",
)
args = parser.parse_args()
max_level = args.max_level
diffs = args.diffs
sub_series = args.sub_series
element_spacing = args.element_spacing
nodes = {0: [0]}


level = 0
while level < max_level:
    level += 1
    step = pow(2, level-1)
    nodes[level] = []
    for parent in nodes[level-1]:
        nodes[level].append(parent + (step * 1))
        nodes[level].append(parent + (step * 2))

for level in nodes.keys():
    max_width = len(str(max(nodes[level]))) + element_spacing
    print(f"Level {level}")
    for node in [str(node) for node in nodes[level]]:
        print(' ' * (max_width - len(node)) + f"{node:>}", end='')
    print()
    series = nodes[level].copy()
    for i in range(0, diffs):
        print(' ' * int(max_width / 2), end='')
        diff = [y - x for x, y in zip(series[:-1], series[1:])]
        for diff_item in [str(x) for x in diff]:
            print(' ' * (max_width - len(diff_item)) + f"{diff_item:>}", end='')
        print()
        for offset in range(0, sub_series):
            print(' ' * int(max_width / 2), end='')
            first = True
            for diff_item in [str(node) for node in diff[offset::2]]:
                space = ' ' * (2 * max_width - len(diff_item))
                if first:
                    space = ' ' * ((offset + 1) * max_width - len(diff_item))
                    first = False
                print(f"{space}{diff_item:>}", end='')
            print()
        series = diff.copy()
    print()
