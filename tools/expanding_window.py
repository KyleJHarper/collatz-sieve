#!/usr/bin/env python3

"""
Try to model the expanding window so it's less error-prone from human math.
Like so:

L0 (+0)                                             0
                                                    |
L1 (+1)                      1______________________|____________________2
                             |                                           |
L2 (+2)           3__________|__________5                     4__________|__________6
                  |                     |                     |                     |
L3 (+4)      7____|_____11         9____|_____13         8____|_____12        10____|_____14
             |          |          |          |          |          |          |          |
L4 (+8)  15__|__23  19__|__27  17__|__25  21__|__29  16__|__24  20__|__28  18__|__26  22__|__30
"""

import argparse
import networkx
import matplotlib.pyplot as plt

parser = argparse.ArgumentParser(description="Build an expanding window graph.")
parser.add_argument(
    '--levels',
    type=int,
    help="How levels deep to go.  Default is %(default)s.",
    default=4,
    action='store',
    dest='levels',
)
parser.add_argument(
    '--file',
    type=str,
    help="If specified, output to the file named.",
    default=None,
    action='store',
    dest='file',
)
args = parser.parse_args()
file = args.file
MAX_LEVELS = args.levels
lefts = {}
for level in range(0, MAX_LEVELS+1):
    lefts[level] = [0]


def add_node(
    graph: networkx.DiGraph,
    positions: dict,
    node: int,
    level: int,
    parent_node: int = None,
):
    # Add the node first.
    graph.add_node(node)
    if parent_node is not None:
        graph.add_edge(parent_node, node)
    # Compute position for printing.
    sibling_count = pow(2, level)
    left = lefts[level][-1] + (1 / (sibling_count + 1))
    lefts[level].append(left)
    positions[node] = (left, MAX_LEVELS - level)
    # Now add its children, if we're allowed.
    if level < MAX_LEVELS:
        step = pow(2, level)
        for branch_factor in [1, 2]:
            child_node = node + step * branch_factor
            add_node(graph=graph, positions=positions, node=child_node, level=level+1, parent_node=node)


graph = networkx.DiGraph()
positions = {}
add_node(graph=graph, positions=positions, node=0, level=0)
options = {
    "font_size": 12,
    "node_size": 500,
    "node_color": "white",
    "edgecolors": "black",
    "linewidths": 1,
    "width": 1,
}
networkx.draw_networkx(G=graph, pos=positions, **options)
ax = plt.gca()
ax.margins(0)
plt.subplots_adjust(left=0, bottom=0, right=1, top=1, wspace=0, hspace=0)
if file is None:
    plt.show()
else:
    plt.savefig(file, bbox_inches='tight', pad_inches=0)
