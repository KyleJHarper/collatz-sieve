"""
Static methods for generating simple sequences and getting stops back, or you can create an entire
Collatz object which will have additional data.
"""

from typing import List
from typing import Self


class Collatz:
    @property
    def initial_value(self):
        return self._initial_value

    @property
    def oe_string(self):
        return self._oe_string

    @property
    def sequence(self):
        return self._sequence

    @property
    def stop_count(self):
        return self._stop_count

    @property
    def step(self):
        return self._step

    @property
    def oe_prefix(self):
        if self._oe_prefix is None:
            self._oe_prefix = self.find_oe_prefix()
        return self._oe_prefix

    @property
    def oe_prefix_3s_value(self):
        return pow(3, self.oe_prefix.count('O'))

    @property
    def oe_prefix_2s_value(self):
        return pow(2, self.oe_prefix.count('E'))

    def __init__(
        self,
        initial_value: int = 0,
        step: int = 1,
    ):
        self._initial_value = initial_value
        self._step = step
        self._sequence = Collatz.generate_sequence(initial_value)
        self._stop_count = len(self._sequence)
        self._oe_string = ''
        for stop in self.sequence:
            self._oe_string += 'E' if stop % 2 == 0 else 'O'
        self._oe_prefix = None

    def __str__(self):
        """ Our to-string method of a collatz should, reasonably, be the initial value of its sequence. """
        return str(self.initial_value)

    def find_oe_prefix(
        self,
        step: int = None,
        max_count: int = 50,
        messages: bool = False,
    ) -> str:
        """
        Starts at `self.initial_value` and creates a series of sequences by, adding `self.step` each
        time or `step` if provided.  Will generate `count` sequences.  If `messages` is enabled,
        additional info will be emitted whenever a sequence has to be removed for terminating before
        the pattern is finished repeating.

        Returns the prefix of Odds and Evens that match the whole series.
        """
        # Generate series of sequences.
        sequences: List[Collatz] = []
        count = 0
        chosen_step = step if step is not None else self.step
        while count < max_count:
            count += 1
            iv = self.initial_value + (chosen_step * (count - 1))
            sequences.append(Collatz(iv))
        # Determine how much of the string repeats.
        index = 0
        done = False
        remove_index = None
        while not done:
            # Remove any items we flagged earlier.
            if remove_index is not None:
                sequences.pop(remove_index)
                remove_index = None
            # If we find a sequence longer than the exemplar (item 0) can test, remove it.
            if index >= len(sequences[0].oe_string):
                if messages:
                    print(f"Need to remove {sequences[0].initial_value} because it's OE pattern ends before repetition ends.")
                remove_index = 0
                continue
            new_char = sequences[0].oe_string[index]
            for i, sequence in enumerate(sequences):
                # If the sequence ends before our pattern ends, remove it.
                if index >= len(sequence.oe_string):
                    if messages:
                        print(f"Need to remove {sequence.initial_value} because it's OE pattern ends before repetition ends.")
                    remove_index = i
                    continue
                if sequence.oe_string[index] != new_char:
                    done = True
                    break
            index += 1
        index -= 1
        prefix = sequences[0].oe_string[0:index]
        if not Collatz.odd_even_alternates(sequences=sequences, index=index):
            raise "All sequences should go between odd and even after a matching prefix ..."
        return prefix

    def is_below_high_water_mark(self) -> bool:
        """
        Determines if the Odd-Even prefix will bring this number below the high-water mark.
        """
        prefix = self.find_oe_prefix()
        numerator = pow(3, prefix.count("O"))
        denominator = pow(2, prefix.count("E"))
        if numerator < denominator:
            return True
        return False

    #
    # Class Methods
    #

    @classmethod
    def generate_sequence(cls, start: int) -> list:
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
            new_oe = sequence.oe_string[index]
            if current_oe == new_oe:
                return False
            current_oe = new_oe
        return True
