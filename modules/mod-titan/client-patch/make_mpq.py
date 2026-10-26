#!/usr/bin/env python3
"""Pack files into an MPQ archive the WoW 3.3.5a client can load (format version 1).

Files are stored uncompressed and unencrypted, which every MPQ reader supports, and a
(listfile) is added so tools like Ladik's MPQ Editor can browse the archive.

Usage: python3 make_mpq.py <out.MPQ> <archive path>=<file> [<archive path>=<file> ...]
Example:
  python3 make_mpq.py patch-E.MPQ "DBFilesClient\\ChrClasses.dbc=out/ChrClasses.dbc" \\
      "DBFilesClient\\CharBaseInfo.dbc=out/CharBaseInfo.dbc"
"""

import struct
import sys
from pathlib import Path

MASK = 0xFFFFFFFF
SECTOR_SIZE_SHIFT = 3                 # sector size 512 << 3 = 4096, the usual value
FILE_EXISTS = 0x80000000
HASH_TABLE_OFFSET, HASH_NAME_A, HASH_NAME_B, HASH_FILE_KEY = 0, 1, 2, 3


def _crypt_table():
    table = [0] * 0x500
    seed = 0x00100001
    for index1 in range(0x100):
        index2 = index1
        for _ in range(5):
            seed = (seed * 125 + 3) % 0x2AAAAB
            high = (seed & 0xFFFF) << 16
            seed = (seed * 125 + 3) % 0x2AAAAB
            table[index2] = high | (seed & 0xFFFF)
            index2 += 0x100
    return table


CRYPT = _crypt_table()


def hash_string(name, hash_type):
    seed1, seed2 = 0x7FED7FED, 0xEEEEEEEE
    for ch in name.upper().encode('ascii'):
        seed1 = (CRYPT[(hash_type << 8) + ch] ^ (seed1 + seed2)) & MASK
        seed2 = (ch + seed1 + seed2 + (seed2 << 5) + 3) & MASK
    return seed1


def encrypt(data, key):
    out = bytearray()
    seed = 0xEEEEEEEE
    for (value,) in struct.iter_unpack('<I', data):
        seed = (seed + CRYPT[0x400 + (key & 0xFF)]) & MASK
        out += struct.pack('<I', value ^ ((key + seed) & MASK))
        key = ((((~key) << 0x15) + 0x11111111) | (key >> 0x0B)) & MASK
        seed = (value + seed + (seed << 5) + 3) & MASK
    return bytes(out)


def build(entries):
    """entries: list of (archive path with backslashes, bytes). Returns the archive bytes."""
    names = [name for name, _ in entries]
    entries = entries + [('(listfile)', ('\r\n'.join(names) + '\r\n').encode('ascii'))]

    hash_size = 16
    while hash_size < len(entries) * 2:
        hash_size *= 2

    body = bytearray()
    blocks = []
    for _, data in entries:
        blocks.append((32 + len(body), len(data), len(data), FILE_EXISTS))
        body += data

    empty = (MASK, MASK, 0xFFFF, 0xFFFF, MASK)
    hashes = [empty] * hash_size
    for block_index, (name, _) in enumerate(entries):
        slot = hash_string(name, HASH_TABLE_OFFSET) & (hash_size - 1)
        while hashes[slot] is not empty:
            slot = (slot + 1) & (hash_size - 1)
        hashes[slot] = (hash_string(name, HASH_NAME_A), hash_string(name, HASH_NAME_B), 0, 0, block_index)

    hash_table = b''.join(struct.pack('<IIHHI', *h) for h in hashes)
    block_table = b''.join(struct.pack('<IIII', *b) for b in blocks)
    hash_offset = 32 + len(body)
    block_offset = hash_offset + len(hash_table)
    archive_size = block_offset + len(block_table)

    header = struct.pack('<4sIIHHIIII', b'MPQ\x1a', 32, archive_size, 0, SECTOR_SIZE_SHIFT,
                         hash_offset, block_offset, hash_size, len(blocks))
    return (header + bytes(body)
            + encrypt(hash_table, hash_string('(hash table)', HASH_FILE_KEY))
            + encrypt(block_table, hash_string('(block table)', HASH_FILE_KEY)))


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    entries = []
    for spec in sys.argv[2:]:
        name, _, source = spec.partition('=')
        if not source:
            sys.exit(f'Bad argument "{spec}", expected <archive path>=<file>')
        entries.append((name.replace('/', '\\'), Path(source).read_bytes()))
    Path(sys.argv[1]).write_bytes(build(entries))
    print(f'Wrote {sys.argv[1]} ({len(entries)} files)')


if __name__ == '__main__':
    main()
