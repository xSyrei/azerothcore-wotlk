# Turn CharacterCreate.xml into a Lua file that creates mock widgets (with $parent names expanded,
# templates applied) and lists every inline script with the widget it belongs to.
import sys, xml.etree.ElementTree as ET
NS = '{http://www.blizzard.com/wow/ui/}'
src, out = sys.argv[1], sys.argv[2]
root = ET.parse(src).getroot()
templates = {e.get('name'): e for e in root.iter() if e.get('virtual') == 'true'}
WIDGETS = {'Frame', 'Button', 'CheckButton', 'EditBox', 'ScrollFrame', 'ModelFFX', 'Texture', 'FontString'}
SPECIAL = {'NormalTexture', 'PushedTexture', 'HighlightTexture', 'CheckedTexture', 'DisabledTexture'}
lines, scripts = [], []

def lua(s): return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'

def walk(el, parent_name, owner):
    tag = el.tag.replace(NS, '')
    name = el.get('name')
    if name and '$parent' in name: name = name.replace('$parent', parent_name or '')
    me = owner
    if tag in WIDGETS | SPECIAL and el.get('virtual') != 'true':
        if not name:
            name = None
        kind = tag
        lines.append(f'make({lua(name) if name else "nil"}, {lua(kind)}, {lua(owner) if owner else "nil"}, {lua(el.get("id") or "0")}, {lua(el.get("text") or "")})')
        if tag not in SPECIAL and tag not in ('Texture', 'FontString'):
            me = name
        tmpl = el.get('inherits')
        if tmpl in templates and name:
            for child in templates[tmpl]:
                walk_child(child, name, name)
    if tag == 'Scripts':
        for s in el:
            scripts.append((owner, s.tag.replace(NS, ''), s.text or ''))
        return
    for child in el:
        walk_child(child, name if (tag in WIDGETS and name) else parent_name, me)

def walk_child(child, parent_name, owner):
    walk(child, parent_name, owner)

for el in root:
    if el.get('virtual') == 'true' or el.tag.replace(NS, '') == 'Script':
        continue
    walk(el, None, None)

with open(out, 'w') as f:
    f.write('\n'.join(lines) + '\n')
    f.write('SCRIPTS = {\n')
    for owner, kind, body in scripts:
        f.write(f'  {{ owner = {lua(owner) if owner else "nil"}, kind = {lua(kind)}, body = [==[{body}]==] }},\n')
    f.write('}\n')
print(len(lines), 'widgets,', len(scripts), 'scripts')
