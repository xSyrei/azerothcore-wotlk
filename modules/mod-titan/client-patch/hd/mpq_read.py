#!/usr/bin/env python3
"""Read files out of the WoW 3.3.5a client's MPQ archives (read only: nothing is ever written to them).

Files are found by name (the hash table), so no listfile is needed. Handles what the 3.3.5a archives
use: format versions 1 and 2 (archives under 4 GB), encrypted hash and block tables, files stored
whole or in sectors, zlib and bzip2 compression, encrypted files (and their fixed keys). Imploded
(PKWARE) and audio-compressed files are not supported: none of the textures and maps need them.

Archives opens a client's archives in the client's own order, so a file comes from the patch that
last changed it.
"""

import bz2
import struct
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from make_mpq import CRYPT, MASK, hash_string  # noqa: E402

HASH_TABLE_OFFSET, HASH_NAME_A, HASH_NAME_B, HASH_FILE_KEY = 0, 1, 2, 3
FILE_IMPLODE, FILE_COMPRESS, FILE_ENCRYPTED, FILE_FIX_KEY = 0x100, 0x200, 0x10000, 0x20000
FILE_SINGLE_UNIT, FILE_SECTOR_CRC, FILE_EXISTS = 0x1000000, 0x4000000, 0x80000000
COMPRESSION_ZLIB, COMPRESSION_BZIP2 = 0x02, 0x10
HASH_EMPTY, HASH_DELETED = 0xFFFFFFFF, 0xFFFFFFFE


def decrypt(data, key):
    out = bytearray()
    seed = 0xEEEEEEEE
    for (value,) in struct.iter_unpack('<I', data[:len(data) // 4 * 4]):
        seed = (seed + CRYPT[0x400 + (key & 0xFF)]) & MASK
        value ^= (key + seed) & MASK
        out += struct.pack('<I', value)
        key = ((((~key) << 0x15) + 0x11111111) | (key >> 0x0B)) & MASK
        seed = (value + seed + (seed << 5) + 3) & MASK
    return bytes(out) + data[len(data) // 4 * 4:]


def decompress(data):
    """A compressed sector: a byte of compression flags, then the data (bzip2 undone first, then zlib)."""
    flags, data = data[0], data[1:]
    if flags & ~(COMPRESSION_ZLIB | COMPRESSION_BZIP2):
        raise ValueError(f'unsupported MPQ compression 0x{flags:02x}')
    if flags & COMPRESSION_BZIP2:
        data = bz2.decompress(data)
    if flags & COMPRESSION_ZLIB:
        data = zlib.decompress(data)
    return data


class Mpq:
    def __init__(self, path):
        self.path = Path(path)
        self.file = open(self.path, 'rb')
        self.base = self._find_header()
        self.file.seek(self.base)
        header = self.file.read(32)
        (_, _, _, _, sector_shift, hash_offset, block_offset, hash_count,
         block_count) = struct.unpack('<4sIIHHIIII', header)
        self.sector_size = 512 << sector_shift
        self.hashes = self._table(hash_offset, hash_count, '(hash table)')
        self.blocks = self._table(block_offset, block_count, '(block table)')

    def _find_header(self):
        """The archive header sits on a 512-byte boundary (after anything prepended to the file)."""
        offset = 0
        while True:
            self.file.seek(offset)
            magic = self.file.read(4)
            if not magic:
                raise ValueError(f'{self.path}: not an MPQ archive')
            if magic == b'MPQ\x1a':
                return offset
            offset += 512

    def _table(self, offset, count, key_name):
        self.file.seek(self.base + offset)
        data = decrypt(self.file.read(count * 16), hash_string(key_name, HASH_FILE_KEY))
        return [struct.unpack_from('<4I', data, i * 16) for i in range(count)]

    def _block(self, name):
        name = name.replace('/', '\\')
        start = hash_string(name, HASH_TABLE_OFFSET) % len(self.hashes)
        a, b = hash_string(name, HASH_NAME_A), hash_string(name, HASH_NAME_B)
        for step in range(len(self.hashes)):
            hash_a, hash_b, _, block = self.hashes[(start + step) % len(self.hashes)]
            if block == HASH_EMPTY:
                return None
            if block != HASH_DELETED and hash_a == a and hash_b == b:
                return self.blocks[block]
        return None

    def has(self, name):
        block = self._block(name)
        return block is not None and block[3] & FILE_EXISTS

    def read(self, name):
        """The file's bytes, or None if the archive does not have it."""
        block = self._block(name)
        if block is None or not block[3] & FILE_EXISTS:
            return None
        offset, packed_size, size, flags = block
        if flags & FILE_IMPLODE:
            raise ValueError(f'{name}: imploded files are not supported')
        key = 0
        if flags & FILE_ENCRYPTED:
            key = hash_string(name.replace('/', '\\').split('\\')[-1], HASH_FILE_KEY)
            if flags & FILE_FIX_KEY:
                key = ((key + offset) ^ size) & MASK
        self.file.seek(self.base + offset)
        raw = self.file.read(packed_size)

        if flags & FILE_SINGLE_UNIT:
            if flags & FILE_ENCRYPTED:
                raw = decrypt(raw, key)
            return decompress(raw) if flags & FILE_COMPRESS and packed_size < size else raw

        if not flags & FILE_COMPRESS:
            if flags & FILE_ENCRYPTED:
                sectors = [raw[i:i + self.sector_size] for i in range(0, len(raw), self.sector_size)]
                raw = b''.join(decrypt(sector, (key + i) & MASK) for i, sector in enumerate(sectors))
            return raw[:size]

        count = (size + self.sector_size - 1) // self.sector_size
        table_size = (count + 1 + (1 if flags & FILE_SECTOR_CRC else 0)) * 4
        table = raw[:table_size]
        if flags & FILE_ENCRYPTED:
            table = decrypt(table, (key - 1) & MASK)
        offsets = struct.unpack(f'<{count + 1}I', table[:(count + 1) * 4])
        out = bytearray()
        for i in range(count):
            sector = raw[offsets[i]:offsets[i + 1]]
            if flags & FILE_ENCRYPTED:
                sector = decrypt(sector, (key + i) & MASK)
            expected = min(self.sector_size, size - i * self.sector_size)
            out += decompress(sector) if len(sector) < expected else sector
        return bytes(out)


# The client's archives, the ones read last first (each patch outranks what it patches).
ARCHIVES = [
    '{locale}/patch-{locale}-3.MPQ', '{locale}/patch-{locale}-2.MPQ', '{locale}/patch-{locale}.MPQ',
    'patch-3.MPQ', 'patch-2.MPQ', 'patch.MPQ',
    '{locale}/lichking-locale-{locale}.MPQ', '{locale}/expansion-locale-{locale}.MPQ',
    '{locale}/locale-{locale}.MPQ', 'lichking.MPQ', 'expansion.MPQ', 'common-2.MPQ', 'common.MPQ',
]


class Archives:
    """A client's Blizzard archives (Data\\...), searched in the client's order. Our own patches
    (patch-enUS-4.MPQ, the HD patch) are left out: the originals are what get upscaled."""

    def __init__(self, data_dir, locale='enUS'):
        self.archives = []
        for pattern in ARCHIVES:
            path = Path(data_dir) / pattern.format(locale=locale)
            if path.exists():
                self.archives.append(Mpq(path))
        if not self.archives:
            raise ValueError(f'{data_dir}: no WoW 3.3.5a archives found (expected common.MPQ and the rest)')

    def read(self, name):
        for archive in self.archives:
            data = archive.read(name)
            if data is not None:
                return data
        return None

    def has(self, name):
        return any(archive.has(name) for archive in self.archives)
