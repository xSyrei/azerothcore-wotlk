#!/usr/bin/env python3
"""Build the Emberforge login screen scene: the Seat of the Pantheon (Antorus) under a star sky.

Usage: python3 build_login_scene.py <out dir>
Writes UI_Emberforge.m2, UI_Emberforge00.skin and the scene's BLP textures.
"""

import sys
from pathlib import Path

import wowexport_scene_to_m2 as scene

HERE = Path(__file__).resolve().parent
SRC = HERE / 'pantheon'
ROOM = '7du_argusraid_pantheon_set0'
PLACEMENTS = SRC / 'room' / f'{ROOM}_ModelPlacementInformation.csv'
DOODADS = ['7du_argusraid_pantheonthrone01', '7az_waterfall05']   # light rays etc. not exported yet
NAME = 'UI_Emberforge'
CLIENT_DIR = 'Interface\\Glues\\Models\\UI_Emberforge'

# Camera: outside the throne ring on the waterfall side, looking across the platform at the main
# throne, a little above floor level (like the Legion raid's entrance view).
CAMERA = ((430.0, 0.0, 140.0), (-60.0, 0.0, 30.0), 1.1)

# Without world lighting the raw textures read as flat bright gold; this darker, teal-leaning
# tint brings the metal close to the lit raid. Glowing details are kept bright (see scene.tinted).
ROOM_TINT = (0.62, 0.55, 0.40)
THRONE_TINT = (0.70, 0.64, 0.55)


def scene_parts():
    parts = scene.wmo_parts(SRC / 'room', ROOM, SRC / 'textures')
    for p in parts:
        if p.blend == 0:
            p.tint = ROOM_TINT
    for doodad in DOODADS:
        model = scene.m2_parts(SRC / 'doodads', doodad)
        if 'throne' in doodad:
            for p in model:
                if p.blend == 0 and not p.flags & 0x2:
                    p.tint = THRONE_TINT
        for position, rotation, scale in scene.placements(PLACEMENTS, doodad):
            parts += scene.placed(model, position, rotation, scale)
    parts.append(scene.sky_dome((0, 0, 0), 1800, HERE.parent / 'interface' / 'login' / 'StarField.png'))
    return parts


def main():
    out = Path(sys.argv[1] if len(sys.argv) > 1 else 'out')
    camera = CAMERA
    if len(sys.argv) > 2:                     # preview another camera: x,y,z tx,ty,tz fov
        a, b, fov = sys.argv[2:5]
        camera = (tuple(map(float, a.split(','))), tuple(map(float, b.split(','))), float(fov))
    stats = scene.build(scene_parts(), out, NAME, CLIENT_DIR, camera)
    print(f'Wrote {NAME}.m2 to {out}:', stats)


if __name__ == '__main__':
    main()
