#!/usr/bin/env python3
"""A small Direct3D 9 shader assembler (ps_3_0): assembly text -> bytecode, and the client's .bls wrapper.

Enough of the instruction set for the water shader: def, dcl_texcoordN / dcl_fog (inputs), dcl_2d / dcl_cube
(samplers), and arithmetic and texld instructions with write masks, swizzles, negation and _sat. Comments start with
';'. The client keeps each shader in a .bls file: a 28-byte header (its last word the bytecode's size), then the
bytecode.
"""

import struct

OPCODES = {
    'mov': 1, 'add': 2, 'sub': 3, 'mad': 4, 'mul': 5, 'rcp': 6, 'rsq': 7, 'dp3': 8, 'dp4': 9, 'min': 10, 'max': 11,
    'slt': 12, 'sge': 13, 'exp': 14, 'log': 15, 'lrp': 18, 'frc': 19, 'pow': 32, 'abs': 35, 'nrm': 36, 'texld': 66,
    'cmp': 88, 'dp2add': 90,
}
REGISTER_TYPES = {'r': 0, 'v': 1, 'c': 2, 's': 10, 'oC': 8}
USAGES = {'texcoord': 5, 'fog': 11}
SAMPLER_TYPES = {'2d': 2, 'cube': 3}
# The header of the client's own psLiquidProcWater.bls (magic, version, one permutation, its flags).
BLS_HEADER = bytes.fromhex('53505847030001000100000080f803000002000000000000')


def register(text):
    for prefix in ('oC', 'r', 'v', 'c', 's'):
        if text.startswith(prefix) and text[len(prefix):].isdigit():
            kind = REGISTER_TYPES[prefix]
            return 0x80000000 | ((kind & 7) << 28) | ((kind & 0x18) << 8) | int(text[len(prefix):])
    raise ValueError(f'unknown register {text}')


def destination(text, saturate=False):
    name, _, mask = text.partition('.')
    bits = 0xF if not mask else sum(1 << 'xyzw'.index(c) for c in mask)
    return register(name) | (bits << 16) | ((1 if saturate else 0) << 20)


def source(text):
    negate = text.startswith('-')
    name, _, swizzle = text.lstrip('-').partition('.')
    swizzle = (swizzle or 'xyzw')
    swizzle = (swizzle + swizzle[-1] * 4)[:4]
    bits = sum('xyzw'.index(c) << (2 * i) for i, c in enumerate(swizzle))
    return register(name) | (bits << 16) | ((1 if negate else 0) << 24)


def floats(values):
    return [struct.unpack('<I', struct.pack('<f', float(v)))[0] for v in values]


def assemble(text):
    words = []
    for line in text.splitlines():
        line = line.split(';')[0].strip()
        if not line:
            continue
        op, _, rest = line.partition(' ')
        args = [a.strip() for a in rest.split(',')] if rest else []
        if op == 'ps_3_0':
            words.append(0xFFFF0300)
        elif op == 'def':
            words += [81 | (5 << 24), destination(args[0])] + floats(args[1:])
        elif op.startswith('dcl_'):
            kind = op[4:]
            if kind in SAMPLER_TYPES:
                words += [31 | (2 << 24), 0x80000000 | (SAMPLER_TYPES[kind] << 27), destination(args[0])]
            else:
                usage = kind.rstrip('0123456789')
                index = int(kind[len(usage):] or 0)
                words += [31 | (2 << 24), 0x80000000 | USAGES[usage] | (index << 16), destination(args[0])]
        else:
            name, saturate = (op[:-4], True) if op.endswith('_sat') else (op, False)
            operands = [destination(args[0], saturate)] + [source(a) for a in args[1:]]
            words += [OPCODES[name] | (len(operands) << 24)] + operands
    words.append(0x0000FFFF)
    return struct.pack(f'<{len(words)}I', *words)


def bls(bytecode):
    return BLS_HEADER + struct.pack('<I', len(bytecode)) + bytecode
