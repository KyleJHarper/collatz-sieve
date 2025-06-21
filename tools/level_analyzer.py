#!/usr/bin/env python3

import argparse

parser = argparse.ArgumentParser(description="Analyze the levels of our expanding window.")
parser.add_argument(
    'max_level',
    type=int,
    help="The level to end at.",
    action='store',
)
args = parser.parse_args()
max_level = args.max_level
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
    print(f"Level {level}: {', '.join([str(node) for node in nodes[level]])}")
