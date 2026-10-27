#!/usr/bin/env python3
"""Convert an image (PNG, ...) to a BLP2 texture the WoW 3.3.5a client can load.

Writes a palettized BLP2 (256 colors, dithered) with a full mipmap chain and an 8-bit alpha
channel. Width and height must be powers of two. Alpha is always written: the 3.3.5a client failed
to load palettized BLPs without it (showing its green missing-texture box instead).

Usage: python3 blp.py <image> <out.blp>
"""

import struct
import sys

from PIL import Image


def _is_pow2(n):
    return n > 0 and n & (n - 1) == 0


def encode(image):
    image = image.convert('RGBA')
    width, height = image.size
    if not (_is_pow2(width) and _is_pow2(height)):
        raise ValueError(f'texture must be a power of two in both directions, got {width}x{height}')

    has_alpha = True
    # Palette from the pixels that show: fully transparent ones (often white or gray fringes in
    # effect textures) would otherwise take colors away from smooth gradients and band them.
    visible = [rgb for rgb, a in zip(image.convert('RGB').getdata(), image.getchannel('A').getdata()) if a > 4]
    source = image.convert('RGB')
    if visible:
        source = Image.new('RGB', (len(visible), 1))
        source.putdata(visible)
    palette_image = source.quantize(256, method=Image.Quantize.MEDIANCUT)
    palette = palette_image.getpalette()[:256 * 3]
    palette += [0] * (256 * 3 - len(palette))

    mips = []
    level, w, h = image, width, height
    while True:
        indices = level.convert('RGB').quantize(palette=palette_image, dither=Image.Dither.FLOYDSTEINBERG)
        data = indices.tobytes()
        if has_alpha:
            data += level.getchannel('A').tobytes()
        mips.append(data)
        if w == 1 and h == 1:
            break
        w, h = max(1, w // 2), max(1, h // 2)
        level = image.resize((w, h), Image.Resampling.LANCZOS)

    header_size = 4 + 4 + 4 + 8 + 16 * 4 * 2 + 256 * 4
    offsets, sizes, position = [], [], header_size
    for data in mips:
        offsets.append(position)
        sizes.append(len(data))
        position += len(data)
    offsets += [0] * (16 - len(offsets))
    sizes += [0] * (16 - len(sizes))

    alpha_depth = 8 if has_alpha else 0
    header = struct.pack('<4sIBBBBII', b'BLP2', 1, 1, alpha_depth, 8 if has_alpha else 0, 1, width, height)
    header += struct.pack('<16I', *offsets) + struct.pack('<16I', *sizes)
    header += b''.join(struct.pack('<BBBB', palette[i * 3 + 2], palette[i * 3 + 1], palette[i * 3], 0)
                       for i in range(256))
    return header + b''.join(mips)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    with Image.open(sys.argv[1]) as image:
        data = encode(image)
    with open(sys.argv[2], 'wb') as f:
        f.write(data)
    print(f'Wrote {sys.argv[2]} ({len(data)} bytes)')


if __name__ == '__main__':
    main()
