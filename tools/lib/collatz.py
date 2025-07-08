"""
Static methods for generating simple sequences and getting stops back, or you can create an entire
Collatz object which will have additional data.
"""

from typing import List
from typing import Self


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
        if self._oe_pattern == '':
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
