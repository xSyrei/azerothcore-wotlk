#!/usr/bin/env python3
"""Build the Emberforge login screen scene: the Seat of the Pantheon (Antorus) under a star sky.

Usage: python3 build_login_scene.py <out dir>
Writes UI_Emberforge.m2, UI_Emberforge00.skin and the scene's BLP textures.
"""

import sys
from pathlib import Path

import numpy as np
from PIL import Image

import wowexport_scene_to_m2 as scene

HERE = Path(__file__).resolve().parent
SRC = HERE / 'pantheon'
ROOM = '7du_argusraid_pantheon_set0'
PLACEMENTS = SRC / 'room' / f'{ROOM}_ModelPlacementInformation.csv'
DOODADS = ['7du_argusraid_pantheonthrone01', '7az_waterfall05']   # light rays etc. not exported yet
NAME = 'UI_Emberforge'
CLIENT_DIR = 'Interface\\Glues\\Models\\UI_Emberforge'

# Camera: low, out past the platform's front edge on its center line, looking up across the floor
# at the main throne, with the arch and both waterfall pillars framing it.
CAMERA = ((280.0, 0.0, 5.0), (-89.0, 0.0, 60.0), 1.4)

# The room and thrones are lit by the scene's own lights (below); tints set their base color after the
# Legion raid: teal-bronze structure, warm gold floor ring and thrones. Glowing details stay bright
# (see scene.tinted) and unlit.
ROOM_TINT = (0.42, 0.58, 0.62)
FLOOR_TINT = (0.95, 0.80, 0.50)
THRONE_TINT = (0.85, 0.66, 0.46)
WATER_TINT = (0.25, 0.70, 1.00)
# Reflection sheen for the room's environment-metal textures (masked by their alpha).
SHEEN = (0.30, 0.42, 0.40)
ART = HERE.parent / 'interface' / 'login'

# Gem glows: Blizzard's icecrown_blueglow_01 spots (placement scale -> glow radius in yards) and the
# Ulduar power orbs set into the arch (not the one on the spire tip, which is out of view here and
# would glow in empty sky).
GLOW_MODELS = {'icecrown_blueglow_01': 6.0, 'ul_power_orb02': 13.0}
# Lights: a warm key light high above the front of the platform (also carrying the cool ambient that
# fills the shadows), and a cyan glow inside the throne ring from the gems and runes.
LIGHTS = [
    scene.point_light((150.0, 0.0, 350.0), ((0.22, 0.32, 0.44), 1.0), ((1.00, 0.86, 0.62), 1.0), (0.0, 900.0)),
    scene.point_light((-20.0, 0.0, 40.0), ((0.0, 0.0, 0.0), 0.0), ((0.35, 0.75, 1.00), 0.9), (40.0, 220.0)),
]

# Sky layers from Argus's Legion skybox textures (models/pantheon/sky), as camera-facing cards in
# front of the star dome, farthest first: (texture, screen position (-1..1 across, -1..1 up, for
# 16:9), distance, half width and height in yards, blend: 2 alpha, 4 additive).
SKY = [
    ('Nebula_Galaxy', (-0.62, 0.62), 985.0, (330.0, 330.0), 2),
    ('Nebula_Cloud1', (0.78, 0.30), 980.0, (300.0, 150.0), 2),
    ('Nebula_Cloud3', (-0.85, -0.35), 975.0, (330.0, 165.0), 2),
    ('Nebula_Cloud4', (0.88, -0.55), 970.0, (260.0, 130.0), 2),
    ('Nebula_Cloud2', (0.30, 0.95), 965.0, (280.0, 140.0), 2),
    ('Nebula_Burst', (0.0, 0.18), 960.0, (300.0, 300.0), 4),
    ('Nebula_Wisp1', (-0.20, 0.55), 955.0, (520.0, 130.0), 4),
    ('Nebula_Wisp2', (0.10, -0.70), 950.0, (600.0, 150.0), 4),
]
SKY_LIMIT = 950.0                       # yards; the star dome is at 1000
# Planet: where it sits on screen, its distance and radius.
PLANET = ((0.32, 0.84), 900.0, 34.0)


def reflection_mask(texture):
    """Whether a texture's alpha is a reflection mask (mostly low) rather than plain opaque."""
    with Image.open(texture) as image:
        return image.mode == 'RGBA' and np.asarray(image.getchannel('A')).mean() < 200


def on_floor(centers, normals):
    """Triangles of the floor ring: upward faces of the platform top inside the throne circle."""
    return ((np.hypot(centers[:, 0], centers[:, 1]) < 75) & (centers[:, 2] > -12) & (centers[:, 2] < 3)
            & (normals[:, 2] > 0.8))


def scene_parts(camera=CAMERA):
    parts = []
    for p in scene.wmo_parts(SRC / 'room', ROOM, SRC / 'textures'):
        if p.blend != 0:
            parts.append(p)
            continue
        floor, rest = scene.split(p, on_floor)
        sheen = SHEEN if reflection_mask(p.texture) else ()
        for q, tint in ((floor, FLOOR_TINT + sheen), (rest, ROOM_TINT + sheen)):
            if q is not None:
                q.tint, q.flags = tint, q.flags & ~scene.UNLIT
                parts.append(q)
    for doodad in DOODADS:
        model = scene.m2_parts(SRC / 'doodads', doodad)
        for p in model:
            if 'throne' in doodad and p.blend == 0 and not p.flags & 0x2:
                p.tint, p.flags = THRONE_TINT, p.flags & ~scene.UNLIT
            elif 'waterfall' in doodad:
                p.tint, p.blend = WATER_TINT, 4             # glowing blue, added onto what is behind
        for position, rotation, scale in scene.placements(PLACEMENTS, doodad):
            parts += scene.placed(model, position, rotation, scale)

    glows = [(position, scale * size) for model, size in GLOW_MODELS.items()
             for position, _, scale in scene.placements(PLACEMENTS, model) if position[2] < 150]
    parts.append(scene.camera_cards(glows, camera, ART / 'GemGlow.png'))

    # Centered on the camera and inside the client's maximum draw distance (farclip 1277).
    parts.append(scene.sky_dome(camera[0], 1000, ART / 'StarField.png', repeat=(8, 4)))
    r, u, _ = scene.camera_basis(camera)
    for texture, (sx, sy), distance, (rx, ry), blend in SKY:
        # A flat card's far corners must stay inside the dome, or the dome hides them. Bring the card
        # closer, scaled down with it so it covers the same part of the screen.
        center = scene.screen_point(camera, sx, sy, distance)
        corners = [center + a * rx * r + b * ry * u for a in (-1, 1) for b in (-1, 1)]
        k = min(1.0, SKY_LIMIT / max(np.linalg.norm(c - np.asarray(camera[0])) for c in corners))
        center = scene.screen_point(camera, sx, sy, distance * k)
        parts.append(scene.camera_cards([(center, (rx * k, ry * k))], camera, SRC / 'sky' / f'{texture}.png',
                                        blend=blend, pull=0, depth_write=False))
    (sx, sy), distance, radius = PLANET
    center = scene.screen_point(camera, sx, sy, distance)
    parts.append(scene.camera_cards([(center, radius)], camera, ART / 'Planet.png', blend=2, pull=0))
    return parts


def build(out_dir, camera=CAMERA):
    """Write the scene (M2, skin, BLPs) into out_dir; returns build stats."""
    return scene.build(scene_parts(camera), out_dir, NAME, CLIENT_DIR, camera, lights=LIGHTS)


def main():
    out = Path(sys.argv[1] if len(sys.argv) > 1 else 'out')
    camera = CAMERA
    if len(sys.argv) > 2:                     # preview another camera: x,y,z tx,ty,tz fov
        a, b, fov = sys.argv[2:5]
        camera = (tuple(map(float, a.split(','))), tuple(map(float, b.split(','))), float(fov))
    stats = build(out, camera)
    print(f'Wrote {NAME}.m2 to {out}:', stats)


if __name__ == '__main__':
    main()
