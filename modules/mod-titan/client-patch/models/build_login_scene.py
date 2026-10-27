#!/usr/bin/env python3
"""Build the Emberforge login screen scene: the Seat of the Pantheon (Antorus) under a star sky.

Usage: python3 build_login_scene.py <out dir>
Writes UI_Emberforge.m2, UI_Emberforge00.skin and the scene's BLP textures.
"""

import sys
from pathlib import Path

import numpy as np

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

# Without world lighting the raw textures read as flat bright gold. Tints stand in for the raid's
# lighting (see the Legion reference): the structure goes dark teal-bronze, while the floor ring and
# the thrones keep warm gold. Glowing details stay bright (see scene.tinted).
ROOM_TINT = (0.21, 0.34, 0.42)
FLOOR_TINT = (0.78, 0.62, 0.38)
THRONE_TINT = (0.58, 0.45, 0.32)
WATER_TINT = (0.25, 0.70, 1.00)
ART = HERE.parent / 'interface' / 'login'

# Gem glows: Blizzard's icecrown_blueglow_01 spots (placement scale -> glow radius in yards) and the
# Ulduar power orbs set into the arch (not the one on the spire tip, which is out of view here and
# would glow in empty sky).
GLOW_MODELS = {'icecrown_blueglow_01': 3.6, 'ul_power_orb02': 9.0}
# Planet: where it sits on screen (-1..1 across, -1..1 up, for 16:9), its distance and radius.
PLANET = ((0.32, 0.84), 900.0, 34.0)


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
        for q, tint in ((floor, FLOOR_TINT), (rest, ROOM_TINT)):
            if q is not None:
                q.tint = tint
                parts.append(q)
    for doodad in DOODADS:
        model = scene.m2_parts(SRC / 'doodads', doodad)
        for p in model:
            if 'throne' in doodad and p.blend == 0 and not p.flags & 0x2:
                p.tint = THRONE_TINT
            elif 'waterfall' in doodad:
                p.tint, p.blend = WATER_TINT, 4             # glowing blue, added onto what is behind
        for position, rotation, scale in scene.placements(PLACEMENTS, doodad):
            parts += scene.placed(model, position, rotation, scale)

    glows = [(position, scale * size) for model, size in GLOW_MODELS.items()
             for position, _, scale in scene.placements(PLACEMENTS, model) if position[2] < 150]
    parts.append(scene.camera_cards(glows, camera, ART / 'GemGlow.png'))

    # Centered on the camera and inside the client's maximum draw distance (farclip 1277).
    parts.append(scene.sky_dome(camera[0], 1000, ART / 'StarField.png', repeat=(8, 4)))
    (sx, sy), distance, radius = PLANET
    r, u, f = scene.camera_basis(camera)
    t = scene.vertical_tan(camera[2], 16 / 9)
    direction = f + r * sx * t * 16 / 9 + u * sy * t
    center = np.asarray(camera[0]) + direction / np.linalg.norm(direction) * distance
    parts.append(scene.camera_cards([(center, radius)], camera, ART / 'Planet.png', blend=2, pull=0))
    return parts


def main():
    out = Path(sys.argv[1] if len(sys.argv) > 1 else 'out')
    camera = CAMERA
    if len(sys.argv) > 2:                     # preview another camera: x,y,z tx,ty,tz fov
        a, b, fov = sys.argv[2:5]
        camera = (tuple(map(float, a.split(','))), tuple(map(float, b.split(','))), float(fov))
    stats = scene.build(scene_parts(camera), out, NAME, CLIENT_DIR, camera)
    print(f'Wrote {NAME}.m2 to {out}:', stats)


if __name__ == '__main__':
    main()
