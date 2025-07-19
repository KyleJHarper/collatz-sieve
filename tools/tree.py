#!/usr/bin/env python3

"""
Try to model the expanding window so it's less error-prone from human math and display as text.
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
from lib.collatz import BinaryTree
import drawsvg
import textwrap


parser = argparse.ArgumentParser(description="Build an expanding window graph.")
parser.add_argument(
    '--levels',
    type=int,
    help="Number of levels deep to go.  Default is %(default)s.",
    default=4,
    action='store',
    dest='levels',
)
parser.add_argument(
    '--show-all',
    help="Enables the display of everything (all the --show-* options).",
    action='store_true',
    dest='show_all',
)
parser.add_argument(
    '--show-constant',
    help="Enables the display of the constant factor for the node, based on its 3^x/2^y value.",
    action='store_true',
    dest='show_constant',
)
parser.add_argument(
    '--show-fraction',
    help="Enables the display of the 3^x/2^y values.",
    action='store_true',
    dest='show_fraction',
)
parser.add_argument(
    '--show-oe',
    help="Enables the display of the Odd-Even pattern in the tree output.",
    action='store_true',
    dest='show_oe',
)
args = parser.parse_args()
MAX_LEVELS = args.levels
show_all = args.show_all
show_constant = args.show_constant
show_fraction = args.show_fraction
show_oe = args.show_oe
if show_all:
    show_constant = True
    show_fraction = True
    show_oe = True


# Build the tree.
tree = BinaryTree(max_levels=MAX_LEVELS)

# Now build the SVG.  Each node will be assigned a rectangle with the number, OE pattern, and
# the power of 3/2 assigned.
NODE_HEIGHT = 300
NODE_WIDTH = 200
NODE_SPACING = int(NODE_WIDTH / 5)
LEVEL_SPACING = int(NODE_HEIGHT / 1)
DRAWING_PADDING = 0
DRAWING_HEIGHT = (NODE_HEIGHT + LEVEL_SPACING) * (tree.max_levels + 1) + (2 * DRAWING_PADDING)
DRAWING_WIDTH = (NODE_WIDTH + NODE_SPACING) * len(tree.levels[tree.max_levels]) + (2 * DRAWING_PADDING)

drawing = drawsvg.Drawing(
    height=DRAWING_HEIGHT,
    width=DRAWING_WIDTH,
)

x = 0
y = DRAWING_PADDING + int(LEVEL_SPACING / 2)
for level in tree.levels.keys():
    nodes = tree.levels[level]
    column_width = int(DRAWING_WIDTH / len(nodes))
    x = DRAWING_PADDING + ((column_width - NODE_WIDTH) / 2)
    for node in nodes:
        # Build the group using a transform to position it.
        group = drawsvg.Group(
            width=NODE_WIDTH,
            height=NODE_HEIGHT,
            transform=f"translate({x}, {y})",
        )
        # Build the rectangle and inner bits with coords relative to the group.
        radius = 0.15
        fill = '#ffffff'
        if node.is_below_high_water_mark:
            fill = '#00ff00'
        rect = drawsvg.Rectangle(
            x=0,
            y=0,
            height=NODE_HEIGHT,
            width=NODE_WIDTH,
            fill=fill,
            stroke='black',
            rx=int(NODE_WIDTH * radius),
            ry=int(NODE_WIDTH * radius),
        )
        group.append(rect)
        # Place the node number and such inside the rectangle.
        rect_padding = 10
        center_x = int(NODE_WIDTH / 2)
        number_font_size = 50
        number_y = number_font_size + rect_padding
        fraction_font_size = 40
        fraction_y = number_y + number_font_size
        oe_font_size = 30
        oe_y = fraction_y + fraction_font_size
        node_number = drawsvg.Text(
            text=str(node.value),
            font_size=number_font_size,
            font_weight='bold',
            text_anchor='middle',
            center=True,
            x=center_x,
            y=number_y,
        )
        group.append(node_number)
        node_fraction = drawsvg.Text(
            text=f"{node.threes_value}/{node.twos_value}",
            font_size=fraction_font_size,
            font_weight='normal',
            text_anchor='middle',
            center=True,
            x=center_x,
            y=fraction_y,
        )
        group.append(node_fraction)
        lines = textwrap.wrap(
            text=node.oe_chain,
            width=int(NODE_WIDTH / oe_font_size) + 2,
        )
        for index, line in enumerate(lines):
            node_oe_pattern = drawsvg.Text(
                text=line,
                font_size=oe_font_size,
                font_weight='normal',
                text_anchor='middle',
                center=True,
                x=center_x,
                y=oe_y + (index * oe_font_size),
            )
            group.append(node_oe_pattern)
        drawing.append(group)
        # Connect the group to the parent's group with a line.
        if node.parent is not None:
            start_x = x + int(column_width / 2) + int(NODE_WIDTH / 2)
            if node.parent.children[1].value == node.value:
                start_x = x - int(column_width / 2) + int(NODE_WIDTH / 2)
            start_y = y - LEVEL_SPACING
            end_x = x + int(NODE_WIDTH / 2)
            end_y = y
            line = drawsvg.Line(
                sx=start_x,
                sy=start_y,
                ex=end_x,
                ey=end_y,
                stroke='black',
                stroke_width=3,
            )
            drawing.append(line)
        # Update x based on the drawing width and the number of nodes.
        x += column_width
    # Bump y for the new level.
    y += NODE_HEIGHT + LEVEL_SPACING

# Save the SVG.
drawing.save_svg('zz_example.svg')
