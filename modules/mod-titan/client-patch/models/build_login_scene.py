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

# Camera: low, just outside the platform's front edge by the left waterfall pillar, looking up
# across the floor at the arch and the throne ring (framed after a wow.export view of the room).
CAMERA = ((205.0, -50.0, -25.0), (-60.0, 45.0, 65.0), 1.4)

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
    # Centered on the camera and inside the client's maximum draw distance (farclip 1277).
    stars = HERE.parent / 'interface' / 'login' / 'StarField.png'
    parts.append(scene.sky_dome(CAMERA[0], 1000, stars, repeat=(8, 4)))
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
