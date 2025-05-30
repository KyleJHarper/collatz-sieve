#!/usr/bin/python3
import argparse
from lib.collatz import Collatz

"""
Builds a pretty table of integers from --start to --end and shows binary and hex formats.
"""

# Get args
parser = argparse.ArgumentParser(description="Build a table of integers in decimal, binary, and hex.")
parser.add_argument(
    '--binary-padding',
    type=int,
    help="How many leading zeros to show in binary (for ease of reading).  Default is %(default)s.",
    default=2,
    action='store',
    dest='binary_padding',
)
parser.add_argument(
    '--binary-split',
    type=int,
    help="How many bits to group together before spacing.  Default is %(default)s.",
    default=8,
    action='store',
    dest='binary_split',
)
parser.add_argument(
    '--breakup-size',
    type=int,
    help="Break the results with an empty row every X rows. 0 (or lower) means don't.  Default is %(default)s.",
    default=0,
    action='store',
    dest='breakup_size',
)
parser.add_argument(
    '--end',
    type=int,
    help="Ending number.  Default is %(default)s.",
    default=20,
    action='store',
    dest='end',
)
parser.add_argument(
    '--step',
    type=int,
    help="Only calcuate every Nth number (step over the others).  1 means all numbers.  Default is %(default)s.",
    default=1,
    action='store',
    dest='step',
)
parser.add_argument(
    '--full-sequence',
    help="Print the full sequence, spanning multiple lines if needed.  See --sequence-width.",
    default=False,
    action='store_true',
    dest='full_sequence',
)
parser.add_argument(
    '--oe-split',
    type=int,
    help="How many odds/evens to group together before spacing.  Default is %(default)s.",
    default=4,
    action='store',
    dest='oe_split',
)
parser.add_argument(
    '--oe-width',
    type=int,
    help="Print this many odd/even markers.  0 disables the column.  Default is %(default)s.",
    default=16,
    action='store',
    dest='oe_width',
)
parser.add_argument(
    '--sequence-width',
    type=int,
    help="Show up to X characters of the sequence.  Default is %(default)s.  See --full-sequence.",
    default=60,
    action='store',
    dest='sequence_width',
)
parser.add_argument(
    '--start',
    type=int,
    help="Starting number.  Default is %(default)s.",
    default=1,
    action='store',
    dest='start',
)
args = parser.parse_args()
start = args.start
end = args.end
binary_padding = args.binary_padding
binary_split = args.binary_split
breakup_size = args.breakup_size
sequence_width = args.sequence_width
binary_group_spacing = 2
full_sequence = args.full_sequence
step = args.step
oe_split = args.oe_split
oe_width = args.oe_width
oe_group_spacing = 1


# Calculate maximum bit size and column widths.
max_bits = 1
while (pow(2, max_bits) < end):
    max_bits += 1
# Pad a few for clarity in the table.
max_bits = 64 if (max_bits + binary_padding) > 64 else max_bits + binary_padding
group_count = int(max_bits/binary_split) + (0 if max_bits % binary_split == 0 else 1)
decimal_width = len(str(end)) if len(str(end)) >= 3 else 3
hex_width = len(hex(end)) if len(hex(end)) >= 3 else 3
sequence_width = max(sequence_width, 10)
binary_width = (group_count * binary_split) + ((group_count - 1) * binary_group_spacing)

# Build the headers and separators.
separator = f"+-{'-' * decimal_width}-+-{'-' * hex_width}-+-{'-' * binary_width}-+-{'-' * sequence_width}-+"
if oe_width > 0:
    separator += f"-{'-' * oe_width}-+"
decimal_header = f"{'Decimal'[:decimal_width]:<{decimal_width}}"
hex_header = f"{'Hex'[:hex_width]:<{hex_width}}"
collatz_header = f"{'Sequence'[:sequence_width]:<{sequence_width}}"
oe_header = f"{'Odd/Even'[:oe_width]:<{oe_width}}"
binary_header = f"{'Binary'[:binary_width]:<{binary_width}}"
binary_key_header = f"| {' ' * decimal_width} | {' ' * hex_width} | {' ' * binary_width} | {' ' * sequence_width} |\n"
binary_key_header += f"| {' ' * decimal_width} | {' ' * hex_width} | "
while max_bits % binary_split != 0:
    max_bits += 1
segments = []
x = max_bits
while x > 0:
    high = x
    while high % binary_split != 0:
        high += 1
    x -= 1
    while x % binary_split != 0:
        x -= 1
    low = x + 1
    dot_count = max(binary_split - len(str(high)) - len(str(low)), 0)
    segments.append(f"{high}{'.' * dot_count}{low}")
binary_key_header += (' ' * binary_group_spacing).join(segments)
binary_key_header += f" | {' ' * sequence_width} |\n"
binary_key_header += f"| {' ' * decimal_width} | {' ' * hex_width} | "
segments.clear()
for i in range(0, group_count, 1):
    segments.append("=" * binary_split)
binary_key_header += (' ' * binary_group_spacing).join(segments)
binary_key_header += f" | {' ' * sequence_width} |"
all_headers = f"| {decimal_header} | {hex_header} | {binary_header} | {collatz_header} |"
if oe_width > 0:
    all_headers += f" {oe_header} |"

# Spit out the table.
print("Table Details")
print(f"Start: {start}, End: {end}, Step: {step}, Max Bits: {max_bits}, Decimal Width: {decimal_width}, Hex Width: {hex_width}")
print(separator)
print(all_headers)
print(separator)
completed = 0
for i in range(start, end + 1, 1):
    if (i - start) % step != 0:
        continue
    completed += 1
    if breakup_size > 0 and completed % breakup_size == 0:
        print(binary_key_header)
    line = "| "
    line += f"{str(i)[:decimal_width]:>{decimal_width}} | "
    line += f"{str(hex(i))[:hex_width]:>{hex_width}} | "
    binary = format(i, f"0{max_bits}b")
    binary_groups = [binary[i:i+binary_split] for i in range(0, len(binary), binary_split)]
    line += f"{(' ' * binary_group_spacing).join(binary_groups)} | "
    # Calculate the stops.
    stops = Collatz.generate_sequence(i)
    # Generate the chunk (or chunks, if full_sequence).  Build the Odd/Even chain too.
    oe_chain = ''
    chunk = ''
    chunks = []
    for stop in stops:
        oe_chain += 'E' if stop % 2 == 0 else 'O'
        if len(chunk) + 2 + len(str(stop)) > sequence_width:
            chunks.append(chunk)
            chunk = f"{stop}, "
            continue
        chunk += f"{stop}, "
    # Trim the last comma and store the final (possibly only) chunk.
    chunk = chunk[:-2]
    chunks.append(chunk)
    # Print out the first chunk no matter what.  Along with the Odd/Even string.
    chunk = chunks[0]
    if len(chunks) > 1 and not full_sequence:
        chunk = chunk[:sequence_width-4] + ' ...'
    line += f"{chunk:<{sequence_width}} |"
    if oe_width > 0:
        oe_groups = [oe_chain[i:i+oe_split] for i in range(0, len(oe_chain), oe_split)]
        oe_formatted = f"{(' ' * oe_group_spacing).join(oe_groups)}"
        if len(oe_formatted) > oe_width:
            oe_formatted = oe_formatted[:oe_width-4] + ' ...'
        line += f" {oe_formatted[:oe_width]:<{oe_width}} |"

    print(line)
    # Print the additional lines if --full-sequence is enabled.
    if full_sequence:
        for chunk in chunks[1:]:
            line = f"| {' ' * decimal_width} | {' ' * hex_width} | {' ' * binary_width} | {chunk:<{sequence_width}} |"
            if oe_width > 0:
                line += f" {' ' * oe_width} |"
            print(line)
print(separator)
