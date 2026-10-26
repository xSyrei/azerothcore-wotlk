# Check every <Anchor relativeTo="X"> points at a region the client has already created when the
# anchoring region is created. The client builds a frame, then its <Layers> regions, then its
# <Frames> children (in document order), with templates expanded into each instance.
import sys, xml.etree.ElementTree as ET
NS = '{http://www.blizzard.com/wow/ui/}'
root = ET.parse(sys.argv[1]).getroot()
templates = {e.get('name'): e for e in root if e.get('virtual') == 'true'}
REGION = {'Frame', 'Button', 'CheckButton', 'EditBox', 'ScrollFrame', 'ModelFFX', 'Texture', 'FontString', 'Slider'}
created, problems = set(), []

def tag(e): return e.tag.replace(NS, '')

def build(el, parent):
    name = (el.get('name') or '').replace('$parent', parent or '')
    if name: created.add(name)
    for a in el.iter(NS + 'Anchor'):
        pass
    own_anchors = [a for anchors in el.findall(NS + 'Anchors') for a in anchors]
    for a in own_anchors:
        rel = (a.get('relativeTo') or '').replace('$parent', parent or '')
        if rel and rel not in created:
            problems.append(f'{name or tag(el)} anchors to {rel}, which does not exist yet')
    sources = [el] + ([templates[el.get('inherits')]] if el.get('inherits') in templates else [])
    for src in sources:                                   # layers first, then frames
        for layers in src.findall(NS + 'Layers'):
            for layer in layers:
                for region in layer:
                    build(region, name)
    for src in sources:
        for frames in src.findall(NS + 'Frames'):
            for child in frames:
                build(child, name)
        for sc in src.findall(NS + 'ScrollChild'):
            for child in sc:
                build(child, name)

for el in root:
    if el.get('virtual') != 'true' and tag(el) in REGION:
        build(el, None)
print('\n'.join(problems) if problems else f'all anchors resolve ({len(created)} regions)')
sys.exit(1 if problems else 0)
