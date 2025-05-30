#!/usr/bin/env python3

import argparse
from lib.collatz import Collatz


parser = argparse.ArgumentParser(description="Analyze the e7 element in my Collatz window16 view by digging into the depth of its cycle and repetitions.")
parser.add_argument(
    '--jumps',
    type=int,
    help="How jumps to do on each node.  Default is %(default)s.",
    default=2,
    action='store',
    dest='jumps',
)
parser.add_argument(
    '--levels',
    type=int,
    help="How levels deep to go.  Default is %(default)s.",
    default=3,
    action='store',
    dest='levels',
)
args = parser.parse_args()
max_jumps = args.jumps
max_levels = args.levels
coverage_map = {}


def calculate_coverage(sequence: Collatz):
    msg = f"Number {sequence.initial_value} (step: {sequence.step}) has prefix: {sequence.oe_prefix}."
    numerator = sequence.oe_prefix_3s_value
    denominator = sequence.oe_prefix_2s_value
    if sequence.is_below_high_water_mark():
        coverage = 100 * 1 / (sequence.step / 16)
        coverage_map[sequence] = coverage
        msg += f"  It is below high-water mark: {numerator}/{denominator}.  We're done.  Coverage is {coverage:.3f}%"
        print(msg)
        return
    # msg += f"It is NOT below high-water mark: {numerator}/{denominator}.  Must check next level."
    # print(msg)
    if sequence.step >= max_step:
        # print(f"  > Max stepped reached for {sequence}.  Quitting.")
        return
    new_step = sequence.step * 2
    sub_sequence_1 = Collatz(sequence.initial_value + sequence.step, step=new_step)
    sub_sequence_2 = Collatz(sequence.initial_value + (sequence.step * 2), step=new_step)
    calculate_coverage(sub_sequence_1)
    calculate_coverage(sub_sequence_2)


#
# Main
#
step = 16
max_step = step * pow(2, max_levels - 1)
E7_ROOT = Collatz(7, step=16)
calculate_coverage(E7_ROOT)
for sequence, coverage in coverage_map.items():
    print(f"Sequence {sequence} (step: {sequence.step}) gave us a {coverage:.3f}% coverage.")
# while level < max_levels:
#     level += 1
#     prefix = E7_ROOT.find_oe_prefix()
#     print(f"Level {level} (step {step}), number {E7_ROOT.initial_value} has prefix: {E7_ROOT.find_oe_prefix()}")
#     odds = E7_ROOT.oe_prefix.count("O")
#     evens = E7_ROOT.oe_prefix.count("E")
#     numerator = pow(3, odds)
#     denominator = pow(2, evens)
#     if E7_ROOT.is_below_high_water_mark():
#         # It's below the high-water mark, we can consider everything below this solved.
#         print(f"It's below the high water mark with {odds} odds and {evens} evens: {numerator}/{denominator}.")
#     else:
#         # It's not below the high-water mark.  Need to step forward and test sub-sections.
#         print(f"It is NOT below the high water mark with {odds} odds and {evens} evens: {numerator}/{denominator}.")




















# level = 0
# graph = networkx.DiGraph()
# graph.add_node(E7_ROOT, data=E7_ROOT)
# sequences: List[Collatz] = [E7_ROOT]
# new_sequences: List[Collatz] = []
# while level < max_levels:
#     level += 1
#     step = 16 * pow(2, level - 1)
#     for sequence in sequences:
#         for i in range(1, max_jumps + 1, 1):
#             new_sequence = Collatz(sequence.initial_value + (step * i), step=step)
#             new_sequences.append(new_sequence)
#             graph.add_node(new_sequence, data=new_sequence)
#             graph.add_edge(sequence, new_sequence)
#     sequences = new_sequences.copy()
#     new_sequences.clear()


# # We now have a graph of E7s, which start at at step 16 and grow by powers of 2.
# root_node = [node for node, degree in graph.in_degree() if degree == 0]
# if len(root_node) == 0:
#     raise Exception(f"Unable to find the root node ... how?  Graph has {len(graph.nodes)} nodes.")
# root_node = root_node[0]



# def print_node(
#         graph: networkx.DiGraph,
#         node: Collatz,
#         depth: int = 1,
#         space: str = '   ',
#         h_link: str = '-',
#         v_link: str = '|',
# ):
#     """
#     Print a directed graph from `node` within `graph`.
#     """
#     # Print ourself.
#     vertical_prefix = f"{space}|" * (depth - 1)
#     horizontal_link = '---'
#     print(f"{vertical_prefix}{horizontal_link}{{{graph.nodes[node]['data']}")
#     # Tell all our neighbors to print too, in order.
#     for neighbor in graph.neighbors(node):
#         print_node(graph=graph, node=neighbor, depth=depth+1, space=space)
