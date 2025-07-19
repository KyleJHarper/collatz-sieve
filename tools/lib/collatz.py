"""
Static methods for generating simple sequences and getting stops back, or you can create an entire
Collatz object which will have additional data.
"""

from typing import List, Self
import math
from decimal import Decimal


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


class Node:
    """ A Node within the expanding window/binary tree concept we're creating. """
    @property
    def value(self):
        return self._value

    @property
    def fraction_portion(self):
        return self._fraction_portion

    @property
    def fraction_constant(self):
        return self._fraction_constant

    @property
    def is_below_high_water_mark(self):
        if (self.fraction_portion * self.value) + self.fraction_constant < self.value:
            return True
        return False

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
        self._fraction_portion = Decimal(self._threes_value) / Decimal(self._twos_value)
        self._fraction_constant = Decimal(0)
        for oe in self._oe_chain:
            if oe == 'E':
                self._fraction_constant /= 2
            else:
                self._fraction_constant = (3 * self._fraction_constant) + 1

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
    def max_levels(self):
        return self._max_levels

    def __init__(self, max_levels: int):
        self._max_levels = max_levels
        # Build the tree here.  This is faster than using the node_at class method in a loop.
        self._root_node = Node(value=0, parent=None)
        self._levels = {
            0: [self._root_node],
        }
        parent_level = 0
        child_level = 0
        while child_level < self._max_levels:
            parent_level = child_level
            child_level += 1
            step = pow(2, parent_level)
            self._levels[child_level] = []
            for parent in self._levels[parent_level]:
                child_values = [parent.value + step, parent.value + (2 * step)]
                for child_value in child_values:
                    child_node = parent.add_child(value=child_value)
                    self._levels[child_level].append(child_node)

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
