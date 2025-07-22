"""
Static methods for generating simple sequences and getting stops back, or you can create an entire
Collatz object which will have additional data.
"""

from typing import List, Self
import math
from decimal import Decimal
import drawsvg
import textwrap


class Collatz:
    @property
    def high_water_mark_index(self):
        """ The index of the `self.sequence` where the first stop goes below `self.initial_value`. """
        if self._high_water_mark_index == 0:
            i = 0
            while i < len(self._sequence):
                i += 1
                if self._sequence[i] < self._initial_value:
                    self._high_water_mark_index = i
                    break
        return self._high_water_mark_index

    @property
    def high_water_mark_value(self):
        return self.sequence[self.high_water_mark_index]

    @property
    def high_water_mark_sequence(self):
        return self.sequence[0:self.high_water_mark_index]

    @property
    def initial_value(self):
        return self._initial_value

    @property
    def oe_pattern(self):
        if self._oe_pattern == '' and self._initial_value > 0:
            for stop in self.sequence:
                self._oe_pattern += 'E' if stop % 2 == 0 else 'O'
        return self._oe_pattern

    @property
    def fg_pattern(self):
        if self._fg_pattern == '':
            self._fg_pattern = Collatz.generate_fg_pattern(self._sequence)
        return self._fg_pattern

    @property
    def sequence(self):
        return self._sequence

    @property
    def stop_count(self):
        return self._stop_count

    @property
    def stopping_time(self):
        return self._stop_count

    @property
    def step(self):
        return self._step

    @property
    def up_down_string(self):
        return self.oe_pattern.replace('E', 'd').replace('O', 'i')[0:-1]

    def __init__(
        self,
        initial_value: int = 0,
    ):
        self._initial_value = initial_value
        self._sequence = Collatz.generate_sequence(initial_value)
        self._stop_count = len(self._sequence)
        self._oe_pattern = ''
        self._fg_pattern = ''
        self._high_water_mark_index = 0

    def __str__(self):
        """ Our to-string method of a collatz should, reasonably, be the initial value of its sequence. """
        return str(self.initial_value)

    def aligns_at(self, other_sequence: Self) -> int:
        """
        Compares `this` sequence to `other_sequence` to find an alignment at the same index.  For example,
        12 and 13 both end at 10 after on the 4th term ([12, 6, 3, 10...] and [13, 40, 20, 10...]).  We
        would return an index of 3.

        Since the indexes must match, this automatically implies the sequences share the same stopping time.

        Returns 0 if not found.  (Terms at index 0 can never match, so this is safe and aligns with `False`.)
        """
        for index in range(1, min(self.stopping_time, other_sequence.stopping_time)):
            if self.sequence[index] == other_sequence.sequence[index]:
                return index
        return 0

    #
    # Class Methods
    #

    @classmethod
    def generate_sequence(cls, start: int) -> list:
        """
        Create a standard list of the stops instead of a full Collatz() object.
        """
        stops = [start]
        while start > 1:
            if start % 2 == 0:
                start = int(start / 2)
            else:
                start = start * 3 + 1
            stops.append(start)
        return stops

    @classmethod
    def odd_even_alternates(cls, sequences: List[Self], index: int) -> bool:
        """
        Tests all `sequences` at `index` to determine if they alternate Odd-Even (or Even-Odd) repeatedly.
        """
        if len(sequences) < 2:
            return False
        current_oe = 'n/a'
        for sequence in sequences:
            new_oe = sequence.oe_pattern[index]
            if current_oe == new_oe:
                return False
            current_oe = new_oe
        return True

    @classmethod
    def generate_fg_pattern(cls, sequence: list) -> str:
        """ Takes a sequence and creates the f-g pattern from it. """
        i = 0
        pattern = ''
        while i < len(sequence):
            if sequence[i] % 2 == 0:
                pattern += 'g'
                i += 1
            else:
                pattern += 'f'
                i += 2
        return pattern

    @classmethod
    def generate_oe_pattern(cls, sequence: list) -> str:
        """ Takes a sequence and creates the OE pattern from it. """
        rv = ""
        for stop in sequence:
            rv += 'E' if stop % 2 == 0 else 'O'
        return rv


class Node:
    """ A Node within the expanding window/binary tree concept we're creating. """
    @property
    def value(self):
        return self._value

    @property
    def fg_n_portion(self):
        return self._fg_n_portion

    @property
    def fg_constant(self):
        return self._fg_constant

    @property
    def fg_total(self):
        return (self.fg_n_portion * self.value) + self.fg_constant

    @property
    def is_below_high_water_mark(self):
        if self.fg_total < self.value:
            return True
        return False

    @property
    def has_high_water_mark_ancestor(self):
        return self._has_high_water_mark_ancestor

    @property
    def children(self):
        return self._children

    @property
    def parent(self):
        return self._parent

    @property
    def oe_chain(self):
        return self._oe_chain

    @property
    def level(self):
        return self._level

    @property
    def threes_value(self):
        return self._threes_value

    @property
    def twos_value(self):
        return self._twos_value

    def __init__(
            self,
            value: int,
            parent: Self,
    ):
        self._value = value
        self._children: List[Self] = []
        self._parent = parent
        # Level is derived from the node's value: floor(log2(N+1))
        self._level = math.floor(math.log(self._value + 1, 2))
        # To get the OE chain and fraction for this node, we get the same number of places as our
        # parent had, and they should match, so we check that first.
        # Generate the Collatz here, but don't save it.  It's a waste of memory one we're out of init().
        sequence = Collatz(value)
        size = 0
        if self._parent is not None:
            size = len(self._parent.oe_chain)
        self._oe_chain = sequence.oe_pattern[0:size]
        if self._parent is not None and self._oe_chain != self._parent.oe_chain:
            raise ValueError(f"The parent oe-chain '{self._parent.oe_chain}' for {self._parent.value} doesn't match ours '{self._oe_chain}' for {self._value}.")
        # Now just grab either 1 place if the current chain ends in "O", otherwise two.
        if self._oe_chain == "" or self._oe_chain[-1:] == "E":
            size += 1
        else:
            size += 2
        self._oe_chain = sequence.oe_pattern[0:size]
        # The threes and twos are a simple count.
        self._threes_value = pow(3, self._oe_chain.count('O'))
        self._twos_value = pow(2, self._oe_chain.count('E'))
        # We need the fractional portion and constant, and their total.
        # Decimal values for real precision, not a float.
        self._fg_n_portion = Decimal(self._threes_value) / Decimal(self._twos_value)
        self._fg_constant = Decimal(0)
        for oe in self._oe_chain:
            if oe == 'E':
                self._fg_constant /= 2
            else:
                self._fg_constant = (3 * self._fg_constant) + 1
        # Track our ancestry's HWM.
        self._has_high_water_mark_ancestor = False
        parent = self._parent
        while parent is not None:
            if parent.is_below_high_water_mark:
                self._has_high_water_mark_ancestor = True
                break
            parent = parent.parent

    def __str__(self):
        return str(self._value)

    def add_child(self, value: int) -> Self:
        """ Adds a child to the node, making this node its parent. """
        child = Node(value=value, parent=self)
        self._children.append(child)
        return child


class BinaryTree:
    """
    A perfect binary tree with `levels` depth.
    """
    @property
    def root_node(self):
        return self._root_node

    @property
    def levels(self):
        return self._levels

    @property
    def level_count(self):
        return self._level_count

    @property
    def max_levels(self):
        return self._max_levels

    def __init__(
        self,
        level_count: int,
        max_levels: int = 0,
    ):
        """
        Build a Binary Tree with `level_count` levels of depth.

        Attributes
        ----------
        level_count : int
            How many levels to build in the initial tree.
        max_levels : int, default 0 (unlimited)
            How many levels the tree can grow before throwing an exception.  0 is unlimited.

        Methods
        -------
        add_level() : None
            Will add another level to the tree.  Exception if over `max_levels`.
        """
        self._level_count = level_count
        self._max_levels = max_levels
        # Build the tree here.  This is faster than using the node_at class method in a loop.
        self._root_node = Node(value=0, parent=None)
        self._levels = {
            0: [self._root_node],
        }
        for i in range(0, level_count):
            self.add_level()

    def add_level(self):
        """ Adds a level to the current tree. """
        # This is easy to do, because we just take the last level and use those as parents.
        # The step is the power of two for that level.
        parent_level = list(self.levels.keys())[-1]
        child_level = parent_level + 1
        self.levels[child_level] = []
        step = pow(2, parent_level)
        for parent in self.levels[parent_level]:
            child_values = [parent.value + step, parent.value + (2 * step)]
            for child_value in child_values:
                child_node = parent.add_child(value=child_value)
                self._levels[child_level].append(child_node)

    def render(
        self,
        file: str,
        node_height: int = 100,
        node_width: int = 75,
        node_spacing_pct: float = 0.2,
        level_spacing_pct: float = 1.0,
        drawing_padding: int = 0,
        format: str = None,
        node_border_radius: float = 0.15,
        node_fill_default: str = '#ffffff',
        node_fill_hwm: str = '#00ff00',
        fill_by_ancestor_hwm: bool = False,
    ):
        """
        Generates an image of the tree in vector (SVG) or raster (PNG) format.  It leans heavily on
        the `drawsvg` library.  Everything is done in pixel units unless otherwise stated.

        All values will be truncated to int() wherever possible to avoid skewing things.  This might
        be a mistake... we'll see.

        Nodes below the high-water mark (or with ancestors below it) will be colored according to
        the value in `node_fill_hwm`.  You can control ancestry consideration with `ancestor_hwm`,
        or disable alternate coloring by assigning the same color from `node_fill_default`.

        Attributes
        -----------
        file: str, required
            The path for the output file.  Will be sent directly to drawing.save_svg/png(), based
            on the file extension, unless `format` is specified.
        node_height : int, default 100
            How tall each node should be.
        node_width : int, default 75
            How wide each node should be.
        node_spacing_pct : float, default 0.2
            The percent of `node_width` to space the last layer of nodes by.
        level_spacing_pct : float, default 1.0
            The percent of `node_height` to space levels by.
        drawing_padding : int, default 0
            How much additional padding to add to the whole drawing.
        format : str, default None
            Which format to output: 'svg' or 'png'.  If None, uses file extension.
        node_border_radius : float, default 0.15
            How much rounding to apply to the x and y corners of the nodes.
        node_fill_default : str, default '#ffffff'
            The default color to fill the nodes with.
        node_fill_hwm : str, default '#00ff00'
            The color to fill the nodes when they meet the high-water mark.
        fill_by_ancestor_hwm : bool, default False
            If an ancestor reaching high-water mark should color the entire node with the
            `node_fill_hwm` value.
        """

        # Calculate our drawing sizes, paddings, and so forth.
        node_spacing = int(node_width * node_spacing_pct)
        level_spacing = int(node_height * level_spacing_pct)
        drawing_height = (node_height + level_spacing) * (self.level_count + 1) + (2 * drawing_padding)
        drawing_width = (node_width + node_spacing) * len(self.levels[self.level_count]) + (2 * drawing_padding)
        rect_padding = int(node_height * 0.1)
        number_font_size = int(node_width * 0.2)
        fg_font_size = int(node_width * 0.15)
        oe_font_size = int(node_width * 0.1)
        line_stroke_width = max(int(node_width * 0.01), 1)
        rect_stroke_width = line_stroke_width + 2

        # Draw the raw canvas.
        drawing = drawsvg.Drawing(
            height=drawing_height,
            width=drawing_width,
        )

        # Attach the boxes, lines, and texts to the drawing.
        x = 0
        y = drawing_padding + int(level_spacing / 2)
        for level in self.levels.keys():
            nodes = self.levels[level]
            column_width = int(drawing_width / len(nodes))
            x = drawing_padding + ((column_width - node_width) / 2)
            for node in nodes:
                # Build the group using a transform to position it.
                group = drawsvg.Group(
                    width=node_width,
                    height=node_height,
                    transform=f"translate({x}, {y})",
                )
                center_x = int(node_width / 2)
                number_y = number_font_size + rect_padding
                fg_fraction_y = number_y + number_font_size
                fg_n_portion_y = fg_fraction_y + fg_font_size
                fg_constant_y = fg_n_portion_y + fg_font_size
                fg_total_y = fg_constant_y + fg_font_size
                oe_y = fg_total_y + fg_font_size
                fill = node_fill_default
                if node.is_below_high_water_mark or (fill_by_ancestor_hwm and node.has_high_water_mark_ancestor):
                    fill = node_fill_hwm
                # Build the rectangle and inner bits with coords relative to the group.
                rect = drawsvg.Rectangle(
                    x=0,
                    y=0,
                    height=node_height,
                    width=node_width,
                    fill=fill,
                    stroke='black',
                    stroke_width=rect_stroke_width,
                    rx=int(node_width * node_border_radius),
                    ry=int(node_width * node_border_radius),
                )
                group.append(rect)
                if node.has_high_water_mark_ancestor:
                    fill = node_fill_hwm
                number_rect = drawsvg.Rectangle(
                    x=rect_stroke_width / 2,
                    y=rect_padding + rect_stroke_width,
                    height=number_font_size,
                    width=node_width - rect_stroke_width,
                    fill=fill,
                    stroke_width=0,
                )
                group.append(number_rect)
                # Place the node number and such inside the rectangle.
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
                    font_size=fg_font_size,
                    font_weight='normal',
                    text_anchor='middle',
                    center=True,
                    x=center_x,
                    y=fg_fraction_y,
                )
                group.append(node_fraction)
                node_fg_n_portion = drawsvg.Text(
                    text=f"{node.fg_n_portion:.7f}*N",
                    font_size=fg_font_size,
                    font_weight='normal',
                    text_anchor='middle',
                    center=True,
                    x=center_x,
                    y=fg_n_portion_y,
                )
                group.append(node_fg_n_portion)
                node_fg_constant = drawsvg.Text(
                    text=f"+ {node.fg_constant:.7f}",
                    font_size=fg_font_size,
                    font_weight='normal',
                    text_anchor='middle',
                    center=True,
                    x=center_x,
                    y=fg_constant_y,
                )
                group.append(node_fg_constant)
                node_fg_total = drawsvg.Text(
                    text=f"= {node.fg_total:.4f}",
                    font_size=fg_font_size,
                    font_weight='bold',
                    text_anchor='middle',
                    center=True,
                    x=center_x,
                    y=fg_total_y,
                )
                group.append(node_fg_total)
                lines = textwrap.wrap(
                    text=node.oe_chain,
                    width=int(node_width / oe_font_size) + 2,
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
                    start_x = x + int(column_width / 2) + int(node_width / 2)
                    if node.parent.children[1].value == node.value:
                        start_x = x - int(column_width / 2) + int(node_width / 2)
                    start_y = y - level_spacing
                    end_x = x + int(node_width / 2)
                    end_y = y
                    line = drawsvg.Line(
                        sx=start_x,
                        sy=start_y,
                        ex=end_x,
                        ey=end_y,
                        stroke='black',
                        stroke_width=line_stroke_width,
                    )
                    drawing.append(line)
                # Update x based on the drawing width and the number of nodes.
                x += column_width
            # Bump y for the new level.
            y += node_height + level_spacing

        # Save the file.
        extension = file[-3:]
        if format is None:
            format = extension
        else:
            if format != extension:
                raise Exception(f"The format you specified is '{format}' but your file extension is '{extension}'.")
        # With a valid format, call save().
        if format == 'svg':
            drawing.save_svg(file)
        elif format == 'png':
            drawing.save_png(file)
        else:
            raise Exception(f"You must specify 'svg' or 'png' for format, not: {format}")

    @classmethod
    def node_at(
        cls,
        level: int,
        position: int,
    ) -> Node:
        """
        Find the value and generate the `Node` for it at the given `level` and `position` it would occur
        in the tree.  Invalid positions (e.g. `position=13, level=2`) throw an exception.

        Returns a `Node` object.
        """
        # Calculate the maximum number of positions.  Error when necessary.
        max_position = pow(2, level)
        if position > max_position:
            raise ValueError(f"You asked for position {position} on level {level}, but it only has {max_position} positions.")
        if position < 1:
            raise ValueError("You cannot request position 0 or lower (negative).  Positions start at 1 (leftmost).")
        # Get the first node's value so we can apply our series summation logic to it.
        first_node_value = pow(2, level) - 1
        # Increases are simple: S1 = ceil((X - 1) / 2) * (2^L-1)
        s1 = math.ceil((position - 1) / 2) * (pow(2, level - 1))
        # Decreases require a sigma-style summation, so we loop here.
        # Formula: [n=2, to L=level] 𝝨 ceil((x - 2^(n-1)) / 2^n) * (2^n - 3) * 2^(L-n)
        s2 = 0
        for n in range(2, level + 1):
            s2 += math.ceil((position - pow(2, n - 1)) / pow(2, n)) * (pow(2, n) - 3) * pow(2, (level - n))
        # The value of the node is simply first node + s1 - s2.
        node_value = first_node_value + s1 - s2
        node = Node(value=node_value, parent=None)
        return node
