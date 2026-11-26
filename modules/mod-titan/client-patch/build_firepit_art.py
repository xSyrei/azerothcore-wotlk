"""The Firepit queue panel's background (AddOns/Emberforge_Firepit/Art/Background.tga): the painting in
icons/ui/FirepitBackground.png (16:9), squeezed into 1024x512 (the addon unsqueezes it to the panel's shape), 24-bit
TGA as the addons' art is. Without the painting, a plain placeholder: a dark arena and a glow where the pit is.

Run: python3 build_firepit_art.py
"""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

HERE = Path(__file__).resolve().parent
SOURCE = HERE / 'icons' / 'ui' / 'FirepitBackground.png'
OUT = HERE / 'AddOns' / 'Emberforge_Firepit' / 'Art' / 'Background.tga'
SIZE = (1024, 512)


def placeholder():
    image = Image.new('RGB', (1672, 941), (22, 10, 8))
    draw = ImageDraw.Draw(image)
    draw.ellipse((-200, 520, 1872, 1300), fill=(38, 18, 14))
    glow = Image.new('RGB', image.size, (0, 0, 0))
    ImageDraw.Draw(glow).ellipse((620, 760, 1050, 880), fill=(255, 90, 20))
    return Image.blend(image, Image.composite(glow, image, glow.convert('L').point(lambda v: 160 if v else 0))
                       .filter(ImageFilter.GaussianBlur(40)), 0.6)


def main():
    image = Image.open(SOURCE).convert('RGB') if SOURCE.exists() else placeholder()
    OUT.parent.mkdir(parents=True, exist_ok=True)
    image.resize(SIZE, Image.LANCZOS).save(OUT, rle=False)
    print(f'wrote {OUT} from {SOURCE if SOURCE.exists() else "the placeholder"}')


if __name__ == '__main__':
    main()
