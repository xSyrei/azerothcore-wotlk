#!/usr/bin/env python3
"""Convert an image (PNG, ...) to a BLP2 texture the WoW 3.3.5a client can load.

Writes a palettized BLP2 (256 colors, dithered) with a full mipmap chain and an 8-bit alpha
channel. Width and height must be powers of two. Alpha is always written: the 3.3.5a client failed
to load palettized BLPs without it (showing its green missing-texture box instead).

Usage: python3 blp.py <image> <out.blp>
"""

import io
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



def resize_straight(image, size):
    """An RGBA image resized with its color and alpha apart. Pillow resizes RGBA with the color weighted by
    alpha, so where alpha is 0 the color comes out black: terrain textures' alpha is their specular, not
    coverage, and their color showed black wherever the specular was 0."""
    color = image.convert('RGB').resize(size, Image.Resampling.LANCZOS)
    color.putalpha(image.getchannel('A').resize(size, Image.Resampling.LANCZOS))
    return color


def _dxt_level(image, fmt):
    """One mip level's DXT blocks (Pillow's encoder; levels under 4 pixels are encoded as one 4x4 block)."""
    if image.width < 4 or image.height < 4:
        image = image.resize((max(4, image.width), max(4, image.height)), Image.Resampling.NEAREST)
    out = io.BytesIO()
    image.save(out, 'DDS', pixel_format=fmt)
    data = out.getvalue()
    return data[128 + (20 if data[84:88] == b'DX10' else 0):]


def encode_dxt(image, mips=True):
    """A DXT-compressed BLP2 (the format of the game's own world textures), with a full mipmap chain:
    DXT1 when the image is opaque, DXT5 with its 8-bit alpha otherwise (terrain textures' specular maps,
    cut-out masks). A cube map strip (six square faces side by side: width six times a power-of-two height, as
    the client's TILESET\\Terrain Cube Maps) is accepted too. mips=False: the top level alone (UI art drawn at its own
    size: a quarter smaller)."""
    image = image.convert('RGBA')
    width, height = image.size
    cube = width == 6 * height and _is_pow2(height)
    if not cube and not (_is_pow2(width) and _is_pow2(height)):
        raise ValueError(f'texture must be a power of two in both directions, got {width}x{height}')
    opaque = image.getchannel('A').getextrema()[0] == 255
    fmt, block = ('DXT1', 8) if opaque else ('DXT5', 16)

    levels, level, w, h = [], image, width, height
    while True:
        blocks = max(1, (w + 3) // 4) * max(1, (h + 3) // 4)
        levels.append(_dxt_level(level, fmt)[:blocks * block])
        if not mips or (w == 1 and h == 1):      # a cube strip too: down to 1x1, as the client's own
            break
        w, h = max(1, w // 2), max(1, h // 2)
        level = resize_straight(image, (w, h))

    header_size = 4 + 4 + 4 + 8 + 16 * 4 * 2 + 256 * 4
    offsets, sizes, position = [], [], header_size
    for data in levels:
        offsets.append(position)
        sizes.append(len(data))
        position += len(data)
    offsets += [0] * (16 - len(offsets))
    sizes += [0] * (16 - len(sizes))
    # encoding 2 (DXT); alpha depth 0 or 8, alpha encoding 0 (DXT1) or 7 (DXT5)
    header = struct.pack('<4sIBBBBII', b'BLP2', 1, 2, 0 if opaque else 8, 0 if opaque else 7, 1 if mips else 0,
                         width, height)
    header += struct.pack('<16I', *offsets) + struct.pack('<16I', *sizes) + b'\0' * 1024
    return header + b''.join(levels)


def encode_raw(image):
    """An uncompressed BLP2 (encoding 3: 32-bit BGRA a pixel), with a full mipmap chain: four times a DXT5's size,
    for fine art DXT's 4x4 blocks would break up (thin rims, filigree)."""
    image = image.convert('RGBA')
    width, height = image.size
    if not (_is_pow2(width) and _is_pow2(height)):
        raise ValueError(f'texture must be a power of two in both directions, got {width}x{height}')
    mips, level, w, h = [], image, width, height
    while True:
        r, g, b, a = level.split()
        mips.append(Image.merge('RGBA', (b, g, r, a)).tobytes())
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
    header = struct.pack('<4sIBBBBII', b'BLP2', 1, 3, 8, 8, 1, width, height)
    header += struct.pack('<16I', *offsets) + struct.pack('<16I', *sizes) + b'\0' * 1024
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
