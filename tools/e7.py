#!/usr/bin/env python3

import argparse
from typing import List
from lib.collatz import Collatz
import networkx

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


#
# Main
#
E7_ROOT = Collatz(7, step=16)
level = 0
graph = networkx.DiGraph()
graph.add_node(E7_ROOT, data=E7_ROOT)
sequences: List[Collatz] = [E7_ROOT]
new_sequences: List[Collatz] = []
while level < max_levels:
    level += 1
    step = 16 * pow(2, level - 1)
    for sequence in sequences:
        for i in range(1, max_jumps + 1, 1):
            new_sequence = Collatz(sequence.initial_value + (step * i), step=step)
            new_sequences.append(new_sequence)
            graph.add_node(new_sequence)
            graph.add_edge(sequence, new_sequence)
    sequences = new_sequences.copy()
    new_sequences.clear()


# We now have a graph of E7s, which start at at step 16 and grow by powers of 2.
root_node = [node for node, degree in graph.in_degree() if degree == 0]
if len(root_node) == 0:
    raise Exception(f"Unable to find the root node ... how?  Graph has {len(graph.nodes)} nodes.")
root_node = root_node[0]

# Print it out.
# space = '    '
# level = 0
# while level < max_levels:
#     level += 1
#     print(f"{space * level}")


networkx.write_network_text(graph, vertical_chains=True)

# s7 = Collatz(7)
# graph = networkx.DiGraph()
# graph.add_node(s7)
# graph.add_node(23)
# graph.add_node(39)
# graph.add_edge(s7, 23)
# graph.add_edge(s7, 39)
# networkx.write_network_text(graph)
