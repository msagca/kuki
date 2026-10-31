# Builds `src/model/stages.blend`: the four places the duel can be fought in, each a set of meshes
# in an `Export` collection. Run by hand, by Blender itself, whenever a stage is to change:
#
#   blender --background --factory-startup --python make_stages.py -- ../src/model/stages.blend
#
# A script for the reason the fighter is one: these are stand-ins made of boxes, cylinders and
# cones, there for the lighting, the weather and the ink to have something to work on, and meant to
# be replaced by stages an artist made. Until then a lantern is a dozen lines below.
#
# Every object is one stage's parts in one material, already where they stand: the geometry is
# built in place around an object at the world origin, so the export -- which takes each object as
# though it stood at the origin -- leaves it where it was built, and the game places every part of
# a stage with no transform at all. The game gives each object its material by name, which is why
# an object holds one and only one: the export writes no materials for a static mesh.
#
# The fight is along X, on the ground at Z = 0, between X = -7 and 7. The camera looks from -Y
# towards +Y, so what is behind the fighters is at positive Y, and Blender's +Y comes out of the
# export as the game's -Z. Nothing is built at negative Y beyond a step or two: it would be behind
# the camera.
import math
import random
import sys

import bmesh
import bpy
from mathutils import Euler, Matrix, Vector


class Stage:
    """The parts of one stage, gathered by material into one bmesh each."""

    def __init__(self, prefix):
        self.prefix = prefix
        self.meshes = {}

    def mesh(self, name):
        key = f"{self.prefix}_{name}"
        if key not in self.meshes:
            self.meshes[key] = bmesh.new()
        return self.meshes[key]

    @staticmethod
    def place(centre, scale=(1, 1, 1), rotation=(0, 0, 0)):
        rotate = Euler(tuple(math.radians(a) for a in rotation)).to_matrix().to_4x4()
        return Matrix.Translation(Vector(centre)) @ rotate @ Matrix.Diagonal((*scale, 1))

    def box(self, name, centre, size, rotation=(0, 0, 0)):
        bmesh.ops.create_cube(self.mesh(name), size=1, matrix=self.place(centre, size, rotation))

    def cylinder(self, name, centre, radius, height, segments=12, rotation=(0, 0, 0), top=None):
        bmesh.ops.create_cone(
            self.mesh(name),
            cap_ends=True,
            segments=segments,
            radius1=radius,
            radius2=radius if top is None else top,
            depth=height,
            matrix=self.place(centre, rotation=rotation),
        )

    def cone(self, name, centre, radius, height, segments=10, rotation=(0, 0, 0)):
        self.cylinder(name, centre, radius, height, segments, rotation, top=0.0)

    def blob(self, name, centre, radius, scale=(1, 1, 1), subdivisions=1):
        bmesh.ops.create_icosphere(
            self.mesh(name),
            subdivisions=subdivisions,
            radius=radius,
            matrix=self.place(centre, scale),
        )

    def beam(self, name, start, end, thickness):
        """A box from one point to another, as thick as given both ways."""
        start, end = Vector(start), Vector(end)
        along = end - start
        rotation = along.to_track_quat("Z", "Y").to_matrix().to_4x4()
        matrix = Matrix.Translation((start + end) / 2) @ rotation @ Matrix.Diagonal((thickness, thickness, along.length, 1))
        bmesh.ops.create_cube(self.mesh(name), size=1, matrix=matrix)

    def build(self, collection):
        for name, bm in sorted(self.meshes.items()):
            bm.normal_update()
            data = bpy.data.meshes.new(name)
            bm.to_mesh(data)
            bm.free()
            obj = bpy.data.objects.new(name, data)
            collection.objects.link(obj)


def windows(stage, lit, dark, x0, x1, y, z0, z1, columns, rows, chance, rng, size=(0.7, 0.9)):
    """A grid of windows on a face looking towards the camera, some lit and the rest dark."""
    for column in range(columns):
        for row in range(rows):
            x = x0 + (x1 - x0) * (column + 0.5) / columns
            z = z0 + (z1 - z0) * (row + 0.5) / rows
            name = lit if rng.random() < chance else dark
            stage.box(name, (x, y - 0.03, z), (size[0], 0.06, size[1]))


def lantern(stage, x, y, stone, glow, roof=None, snow=None):
    """A stone lantern: a base, a post, a lit firebox, a hat and a knob on top."""
    roof = roof or stone
    stage.cylinder(stone, (x, y, 0.15), 0.42, 0.3, segments=6)
    stage.cylinder(stone, (x, y, 0.9), 0.14, 1.2, segments=8)
    stage.box(stone, (x, y, 1.55), (0.7, 0.7, 0.12))
    stage.box(glow, (x, y, 1.82), (0.4, 0.4, 0.42))
    for dx, dy in ((-1, -1), (1, -1), (-1, 1), (1, 1)):
        stage.box(stone, (x + dx * 0.25, y + dy * 0.25, 1.82), (0.1, 0.1, 0.44))
    stage.cylinder(roof, (x, y, 2.18), 0.62, 0.32, segments=4, rotation=(0, 0, 45), top=0.12)
    stage.blob(stone, (x, y, 2.42), 0.1)
    if snow:
        stage.cylinder(snow, (x, y, 2.28), 0.52, 0.12, segments=4, rotation=(0, 0, 45), top=0.18)


def roof(stage, name, centre, width, depth, height, overhang=0.6, cap=None):
    """A gabled roof, ridge along X: two slopes of thin slab meeting at the top."""
    x, y, z = centre
    half = depth / 2 + overhang
    angle = math.degrees(math.atan2(height, half))
    length = math.hypot(half, height) + 0.1
    for side in (-1, 1):
        stage.box(
            name,
            (x, y + side * half / 2, z + height / 2),
            (width + overhang * 2, length, 0.18),
            rotation=(side * angle, 0, 0),
        )
    stage.box(cap or name, (x, y, z + height + 0.05), (width + overhang * 2 + 0.2, 0.3, 0.2))


def build_alley(rng):
    """A back street at night in the rain: shopfronts, neon, wires and towers going up into cloud."""
    s = Stage("alley")
    s.box("asphalt", (0, 8, -0.15), (90, 46, 0.3))
    # Paint on the road, worn and broken, which the wet makes into lines of reflected light.
    for x in range(-30, 31, 4):
        s.box("paint", (x, 2.4, 0.003), (2.2, 0.12, 0.01))
    for x in range(-6, 7, 1):
        s.box("paint", (x * 0.9 + 14, -1.4, 0.003), (0.5, 2.6, 0.01))
    s.box("curb", (0, 4.05, 0.08), (90, 0.2, 0.16))
    s.box("sidewalk", (0, 5.3, 0.07), (90, 2.4, 0.14))
    # The near row of buildings, side by side along the back of the street.
    x = -34.0
    walls = ["wall_brick", "wall_teal", "wall_plaster"]
    neon = ["neon_pink", "neon_cyan", "neon_amber"]
    while x < 34:
        width = rng.uniform(5.0, 8.5)
        height = rng.uniform(9.0, 17.0)
        centre = x + width / 2
        wall = walls[rng.randrange(len(walls))]
        s.box(wall, (centre, 9.5, height / 2), (width - 0.1, 6.0, height))
        # A ledge at each floor, which the ink picks out as lines across the facade.
        for floor in range(1, int(height / 3)):
            s.box("trim", (centre, 6.46, floor * 3.0 + 0.2), (width - 0.1, 0.16, 0.12))
        # The shopfront: a lit window, a door, an awning over both.
        s.box("shop_glow", (centre - width * 0.12, 6.48, 1.35), (width * 0.5, 0.05, 1.9))
        s.box("frame", (centre - width * 0.12, 6.45, 0.3), (width * 0.54, 0.1, 0.18))
        s.box("frame", (centre - width * 0.12, 6.45, 2.36), (width * 0.54, 0.1, 0.14))
        s.box("door", (centre + width * 0.28, 6.47, 1.1), (1.0, 0.08, 2.2))
        awning = ["awning_red", "awning_blue", "awning_green"][rng.randrange(3)]
        s.box(awning, (centre - width * 0.05, 6.0, 2.75), (width * 0.8, 1.2, 0.08), rotation=(-18, 0, 0))
        windows(s, "windows_lit", "windows_dark", centre - width / 2 + 0.4, centre + width / 2 - 0.4, 6.5, 3.4, height - 0.6, max(2, int(width / 1.6)), max(1, int((height - 3.4) / 2.6)), 0.35, rng)
        # A sign hung off the wall towards the street, and a band of neon over the shop.
        tube = neon[rng.randrange(len(neon))]
        if rng.random() < 0.75:
            sx = centre + rng.uniform(-width * 0.3, width * 0.3)
            s.box("sign_board", (sx, 6.2, 5.2), (0.75, 0.16, 2.8))
            s.box(tube, (sx, 6.1, 5.2), (0.58, 0.05, 2.5))
            for i in range(4):
                s.box("sign_board", (sx, 6.06, 4.25 + i * 0.62), (0.5, 0.05, 0.08))
        s.box("sign_board", (centre - width * 0.1, 6.35, 3.25), (width * 0.62, 0.12, 0.62))
        s.box(neon[(neon.index(tube) + 1) % 3], (centre - width * 0.1, 6.27, 3.25), (width * 0.56, 0.04, 0.42))
        # Air conditioners and the pipes that feed them.
        for _ in range(rng.randrange(1, 4)):
            ax = centre + rng.uniform(-width * 0.4, width * 0.4)
            az = rng.uniform(4.0, height - 1.0)
            s.box("metal", (ax, 6.2, az), (0.9, 0.6, 0.6))
            s.box("grille", (ax, 5.89, az), (0.7, 0.04, 0.45))
        s.cylinder("pipe", (x + 0.25, 6.38, height / 2), 0.07, height, segments=6)
        x += width
    # A fire escape zig-zagging up one of them.
    for floor in range(1, 5):
        z = floor * 3.0
        s.box("iron", (-12.5, 5.7, z), (3.2, 1.4, 0.06))
        s.box("iron", (-12.5, 5.02, z + 0.5), (3.2, 0.04, 0.04))
        for post in (-14.0, -11.0):
            s.beam("iron", (post, 5.02, z), (post, 5.02, z + 0.5), 0.04)
        side = -1 if floor % 2 else 1
        s.beam("iron", (-12.5 - side * 1.4, 5.9, z), (-12.5 + side * 1.4, 5.9, z + 3.0), 0.14)
    # Wires strung across between poles, sagging.
    for px in (-10.0, 10.0, 28.0, -28.0):
        s.cylinder("pole", (px, 4.6, 4.75), 0.13, 9.5, segments=8)
        s.box("pole", (px, 4.6, 8.8), (1.8, 0.12, 0.12))
        s.box("transformer", (px + 0.4, 4.85, 7.6), (0.5, 0.5, 0.8))
    for z, sag in ((8.75, 0.5), (8.55, 0.7), (8.9, 0.35)):
        for a, b in ((-28.0, -10.0), (-10.0, 10.0), (10.0, 28.0)):
            steps = 8
            for i in range(steps):
                t0, t1 = i / steps, (i + 1) / steps
                p0 = (a + (b - a) * t0, 4.6, z - sag * 4 * t0 * (1 - t0))
                p1 = (a + (b - a) * t1, 4.6, z - sag * 4 * t1 * (1 - t1))
                s.beam("cable", p0, p1, 0.035)
    # A street lamp, a vending machine and the rubbish beside it.
    s.cylinder("pole", (6.5, 4.4, 2.8), 0.08, 5.6, segments=8)
    s.beam("pole", (6.5, 4.4, 5.55), (6.5, 3.2, 5.8), 0.08)
    s.box("lamp", (6.5, 3.1, 5.7), (0.5, 0.3, 0.12))
    s.box("vending_body", (-3.6, 5.95, 0.95), (1.0, 0.8, 1.9))
    s.box("vending_glow", (-3.6, 5.53, 1.15), (0.82, 0.04, 1.2))
    s.box("vending_body", (-2.5, 5.95, 0.95), (1.0, 0.8, 1.9))
    s.box("vending_glow", (-2.5, 5.53, 1.15), (0.82, 0.04, 1.2))
    for i in range(6):
        s.blob("bags", (2.2 + rng.uniform(-0.6, 0.6), 5.6 + rng.uniform(-0.2, 0.3), 0.3), rng.uniform(0.3, 0.42), (1, 1, 0.8))
    s.box("bins", (3.6, 5.7, 0.55), (1.2, 0.8, 1.1))
    s.box("grille", (-5.0, 1.0, 0.004), (1.0, 0.6, 0.012))
    # The towers behind: dark, tall, lit in rows, with a screen on one of them.
    for _ in range(26):
        tx = rng.uniform(-60, 60)
        ty = rng.uniform(22, 55)
        width = rng.uniform(6, 13)
        height = rng.uniform(22, 60)
        s.box("tower", (tx, ty + width / 2, height / 2), (width, width, height))
        windows(s, "windows_lit" if rng.random() < 0.5 else "windows_cool", "tower_glass", tx - width / 2 + 0.6, tx + width / 2 - 0.6, ty, 2, height - 1, int(width / 1.4), int(height / 2.2), 0.3, rng, size=(0.6, 0.8))
    s.box("tower", (-4, 30, 22), (14, 6, 44))
    s.box("screen", (-4, 26.9, 24), (10, 0.1, 6))
    s.box("neon_pink", (-4, 26.85, 20.7), (10, 0.1, 0.18))
    s.box("neon_pink", (-4, 26.85, 27.3), (10, 0.1, 0.18))
    return s


def build_temple(rng):
    """A temple courtyard at sunset: stone underfoot, lanterns, a gate, blossom and the hills beyond."""
    s = Stage("temple")
    s.box("gravel", (0, 20, -0.12), (140, 70, 0.2))
    # The path the fight is on, laid in slabs, with the joints between them for the ink to find.
    x = -24.0
    while x < 24:
        for row, depth in ((-1, 1.3), (0, 1.3), (1, 1.3)):
            width = rng.choice((1.1, 1.3, 1.6))
            shade = "stone" if rng.random() < 0.7 else "stone_dark"
            s.box(shade, (x + width / 2 + (0.3 if row == 0 else 0), row * 1.34, -0.05), (width - 0.05, depth - 0.05, 0.1))
        x += 1.3
    # Stairs up to the hall, a stone platform and the hall itself, far enough back that the whole of
    # it sits in the frame above the gate's crossbeams.
    h = 12.0
    for i in range(6):
        s.box("stone", (0, h + 19.0 + i * 0.4, 0.15 + i * 0.3), (6.0, 0.6, 0.3 + i * 0.6))
    s.box("stone_dark", (0, h + 25, 0.95), (22, 10, 1.9))
    for px in (-9, -5, -1.5, 1.5, 5, 9):
        s.cylinder("vermilion", (px, h + 20.6, 3.9), 0.3, 4.0, segments=10)
    s.box("plaster", (0, h + 25.2, 3.9), (17.5, 8, 4.0))
    s.box("wood", (0, h + 21.15, 2.15), (18.5, 0.3, 0.3))
    s.box("wood", (0, h + 21.15, 5.6), (19.0, 0.4, 0.4))
    for px in (-6, -2, 2, 6):
        s.box("shoji", (px, h + 21.2, 3.7), (3.0, 0.08, 2.7))
    roof(s, "roof_tile", (0, h + 25, 6.0), 19.0, 10.0, 3.2, overhang=1.6, cap="roof_ridge")
    roof(s, "roof_tile", (0, h + 25, 9.6), 11.0, 6.0, 2.0, overhang=1.0, cap="roof_ridge")
    s.box("plaster", (0, h + 25, 8.4), (11.5, 6, 2.4))
    # The gate between the path and the hall.
    g = 16.0
    for px in (-2.8, 2.8):
        s.cylinder("vermilion", (px, g, 3.2), 0.28, 6.4, segments=12)
        s.cylinder("stone_dark", (px, g, 0.25), 0.4, 0.5, segments=12)
    s.box("vermilion", (0, g, 5.3), (7.6, 0.42, 0.42))
    s.box("vermilion", (0, g, 6.45), (9.0, 0.6, 0.45))
    s.box("black", (0, g, 6.85), (9.6, 0.75, 0.3))
    s.box("vermilion", (0, g, 5.9), (0.5, 0.35, 0.9))
    # Lanterns along the path, and a low fence behind them with the gate's opening left in it.
    for lx in (-11, -5.5, 5.5, 11):
        lantern(s, lx, 3.2, "stone", "lantern_glow")
    for side in (-1, 1):
        for i in range(14):
            fx = side * (4.5 + i * 1.3)
            s.box("wood", (fx, 6.0, 0.5), (0.14, 0.14, 1.0))
        s.box("wood", (side * 13.0, 6.0, 0.85), (17.0, 0.1, 0.12))
        s.box("wood", (side * 13.0, 6.0, 0.4), (17.0, 0.1, 0.1))
    # Cherry trees: a crooked trunk and a crown of blossom.
    for tx, ty, height in ((-9.5, 7.5, 4.5), (10.5, 8.0, 5.0), (-17, 13, 5.5), (17, 14, 5.0), (-24, 20, 6.0), (23, 22, 6.0)):
        lean = rng.uniform(-0.6, 0.6)
        s.beam("bark", (tx, ty, 0), (tx + lean, ty, height * 0.6), 0.45)
        s.beam("bark", (tx + lean, ty, height * 0.6), (tx + lean * 2.2, ty + 0.3, height), 0.3)
        s.beam("bark", (tx + lean, ty, height * 0.65), (tx - 1.4, ty - 0.2, height * 0.9), 0.2)
        for _ in range(9):
            s.blob(
                "blossom",
                (tx + lean * 1.5 + rng.uniform(-2.2, 2.2), ty + rng.uniform(-1.2, 1.2), height + rng.uniform(-0.6, 1.0)),
                rng.uniform(0.9, 1.5),
                (1, 1, 0.75),
                subdivisions=1,
            )
    # A pagoda off to the side, and the hills behind everything.
    for level in range(5):
        size = 5.0 - level * 0.6
        z = 2.0 + level * 2.6
        s.box("plaster", (-34, 40, z), (size * 0.7, size * 0.7, 2.0))
        roof(s, "roof_tile", (-34, 40, z + 1.0), size, size, 0.8, overhang=0.9, cap="roof_ridge")
    s.cylinder("black", (-34, 40, 16.5), 0.1, 4.0, segments=6)
    for mx, my, radius, height in ((-80, 150, 60, 40), (-10, 190, 80, 55), (70, 160, 65, 45), (130, 210, 90, 60), (-140, 220, 90, 50)):
        s.cone("hills", (mx, my, height / 2 - 2), radius, height, segments=9)
    for _ in range(30):
        s.blob("trees_far", (rng.uniform(-80, 80), rng.uniform(45, 80), rng.uniform(1, 3)), rng.uniform(3, 6), (1.6, 1, 0.8))
    return s


def build_rooftop(rng):
    """A roof at dusk among the towers: a water tank, a mast, a billboard and the city lighting up."""
    s = Stage("roof")
    s.box("concrete", (0, 1.5, -0.25), (34, 15, 0.5))
    for x in range(-16, 17, 3):
        s.box("seam", (x, 1.5, 0.003), (0.08, 15, 0.01))
    for y in (-3, 0, 3, 6):
        s.box("seam", (0, y, 0.003), (34, 0.08, 0.01))
    # The parapet round the edge, with a rail on top of the back of it.
    s.box("parapet", (0, 8.2, 0.55), (34, 0.45, 1.1))
    for side in (-1, 1):
        s.box("parapet", (side * 17.2, 2.0, 0.55), (0.45, 12.8, 1.1))
    s.box("rail", (0, 8.2, 1.65), (34, 0.07, 0.07))
    for x in range(-16, 17, 2):
        s.box("rail", (x, 8.2, 1.38), (0.06, 0.06, 0.55))
    # The water tank on its stand.
    for dx, dy in ((-1, -1), (1, -1), (-1, 1), (1, 1)):
        s.box("steel", (-10 + dx * 1.1, 5.5 + dy * 1.1, 1.0), (0.2, 0.2, 2.0))
    s.box("steel", (-10, 5.5, 2.05), (2.8, 2.8, 0.15))
    s.cylinder("tank", (-10, 5.5, 3.8), 1.5, 3.4, segments=16)
    s.cone("tank_roof", (-10, 5.5, 5.85), 1.6, 0.8, segments=16)
    for z in (2.7, 3.8, 4.9):
        s.cylinder("steel", (-10, 5.5, z), 1.53, 0.08, segments=16)
    # Plant on the roof: fans, ducts, a stair housing.
    for i, (ax, ay) in enumerate(((7.5, 5.5), (10, 5.5), (12.5, 5.0))):
        s.box("metal", (ax, ay, 0.7), (2.0, 1.6, 1.4))
        s.cylinder("grille", (ax, ay, 1.42), 0.6, 0.05, segments=16)
    s.beam("metal", (6.5, 5.5, 1.2), (6.5, 7.8, 1.2), 0.5)
    s.box("parapet", (14.5, 4.5, 1.5), (3.0, 3.0, 3.0))
    s.box("door", (14.5, 2.98, 1.1), (1.0, 0.06, 2.1))
    s.box("lamp", (14.5, 2.9, 2.4), (0.4, 0.2, 0.15))
    # A mast with a light on top that the game blinks.
    for dx, dy in ((-0.3, -0.3), (0.3, -0.3), (0.0, 0.35)):
        s.beam("steel", (3.5 + dx, 6.8 + dy, 0), (3.5 + dx * 0.2, 6.8 + dy * 0.2, 12), 0.08)
    for z in range(1, 12, 2):
        s.box("steel", (3.5, 6.8, z), (0.5, 0.5, 0.05))
    s.blob("beacon", (3.5, 6.8, 12.2), 0.16)
    # The billboard on the building behind, lit from below.
    for bx in (-6, -1):
        s.box("steel", (bx, 11, 0.5), (0.2, 0.2, 7))
    s.box("steel", (-3.5, 11.05, 4.6), (8.6, 0.3, 3.4))
    s.box("billboard", (-3.5, 10.85, 4.6), (8.0, 0.1, 2.9))
    # The building across the street, its roof below this one's, so the parapet is an edge with a
    # drop behind it rather than a wall.
    s.box("building_near", (0, 16, -11), (40, 9, 18))
    # The city: towers in every direction, darker and bluer the further off, with windows lit.
    for _ in range(70):
        tx = rng.uniform(-110, 110)
        ty = rng.uniform(25, 140)
        width = rng.uniform(7, 16)
        # Lower near the roof and taller further off, so the near towers sit under the horizon and the
        # sky over them is the dusk's.
        height = rng.uniform(8, 34) * (0.45 + ty / 90)
        base = -20
        name = "tower" if ty < 70 else "tower_far"
        s.box(name, (tx, ty + width / 2, base + height / 2), (width, width, height))
        if rng.random() < 0.15:
            s.box("steel", (tx, ty + width / 2, base + height + 2), (0.3, 0.3, 4))
            s.blob("beacon", (tx, ty + width / 2, base + height + 4.2), 0.3)
        windows(s, "windows_lit" if rng.random() < 0.6 else "windows_cool", "glass", tx - width / 2 + 0.6, tx + width / 2 - 0.6, ty, base + 2, base + height - 1, int(width / 1.6), int(height / 2.6), 0.4, rng, size=(0.8, 1.0))
    return s


def build_shrine(rng):
    """A mountain shrine in snow: a hall under a white roof, pines, a fire, and peaks behind."""
    s = Stage("shrine")
    s.box("snow", (0, 30, -0.15), (200, 90, 0.3))
    for i in range(18):
        s.blob("snow", (rng.uniform(-30, 30), rng.uniform(6, 30), -0.1), rng.uniform(1.0, 2.6), (2.2, 1.4, 0.35))
    # A path of stones, half buried.
    x = -20.0
    while x < 20:
        width = rng.uniform(0.9, 1.5)
        s.box("stone", (x + width / 2, rng.uniform(-0.6, 0.6), -0.02), (width - 0.12, rng.uniform(0.8, 1.2), 0.06))
        x += width
    # The hall on its stilts, snow on its roof.
    s.box("wood_dark", (0, 18, 0.6), (14, 9, 0.3))
    for px in (-6.5, -3, 0, 3, 6.5):
        for py in (13.8, 22.2):
            s.box("wood_dark", (px, py, 0.3), (0.3, 0.3, 0.9))
            s.box("wood", (px, py, 2.7), (0.32, 0.32, 4.0))
    s.box("plaster", (0, 18, 2.75), (12, 7.6, 4.0))
    for px in (-4.5, -1.5, 1.5, 4.5):
        s.box("shoji_lit", (px, 14.15, 2.4), (2.4, 0.08, 2.4))
    s.box("wood", (0, 14.0, 4.85), (13.2, 0.3, 0.3))
    roof(s, "roof", (0, 18, 4.9), 13.0, 9.0, 2.8, overhang=1.4, cap="wood_dark")
    roof(s, "snow", (0, 18, 5.12), 12.6, 8.4, 2.8, overhang=1.2, cap="snow")
    for i in range(4):
        s.box("stone", (0, 12.0 + i * 0.45, 0.12 + i * 0.12), (4.0, 0.5, 0.24 + i * 0.24))
    # A small gate, and a rope between two posts with paper hanging from it.
    for px in (-2.2, 2.2):
        s.cylinder("vermilion", (px, 8.5, 2.2), 0.2, 4.4, segments=10)
    s.box("vermilion", (0, 8.5, 3.6), (5.2, 0.3, 0.3))
    s.box("vermilion", (0, 8.5, 4.45), (6.2, 0.45, 0.35))
    s.box("snow", (0, 8.5, 4.7), (6.4, 0.5, 0.14))
    for px in (-9.0, 9.0):
        s.cylinder("wood", (px, 7.5, 1.65), 0.14, 3.3, segments=8)
        s.box("snow", (px, 7.5, 3.35), (0.3, 0.3, 0.1))
    for i in range(16):
        t = i / 16
        x0, x1 = -9 + 18 * t, -9 + 18 * (t + 1 / 16)
        z0, z1 = 2.1 - 0.6 * 4 * t * (1 - t), 2.1 - 0.6 * 4 * (t + 1 / 16) * (1 - t - 1 / 16)
        s.beam("rope", (x0, 7.5, z0 + 0.9), (x1, 7.5, z1 + 0.9), 0.09)
        if i % 3 == 1:
            s.box("paper", ((x0 + x1) / 2, 7.5, z0 + 0.6), (0.16, 0.02, 0.45), rotation=(0, 8, 0))
    for lx in (-6, 6):
        lantern(s, lx, 4.0, "stone", "lantern_glow", snow="snow")
    # The fire, its stones and its logs.
    for i in range(8):
        angle = i / 8 * math.tau
        s.blob("stone", (-7.5 + math.cos(angle) * 0.75, 2.0 + math.sin(angle) * 0.75, 0.1), 0.24, (1, 1, 0.7))
    for i in range(5):
        angle = i / 5 * math.tau
        s.beam("wood_dark", (-7.5 + math.cos(angle) * 0.55, 2.0 + math.sin(angle) * 0.55, 0.05), (-7.5, 2.0, 0.7), 0.12)
    s.blob("embers", (-7.5, 2.0, 0.15), 0.4, (1, 1, 0.4))
    # Pines, a crowd of them, snow on every tier.
    for _ in range(46):
        side = rng.choice((-1, 1))
        tx = side * rng.uniform(9, 45)
        ty = rng.uniform(7, 45)
        if abs(tx) < 12 and ty < 25:
            continue
        height = rng.uniform(6, 13)
        s.cylinder("bark", (tx, ty, 1.0), 0.25, 2.0, segments=6)
        tiers = 4
        for tier in range(tiers):
            radius = height * 0.32 * (1 - tier / (tiers + 0.5))
            z = 1.5 + tier * height / (tiers + 1)
            s.cone("pine", (tx, ty, z + height * 0.18), radius, height * 0.38, segments=8)
            s.cone("snow", (tx, ty, z + height * 0.27), radius * 0.62, height * 0.22, segments=8)
    for mx, my, radius, height in ((-90, 140, 70, 75), (-20, 170, 80, 95), (60, 150, 70, 80), (130, 190, 90, 90), (-150, 200, 90, 70)):
        s.cone("rock", (mx, my, height / 2 - 5), radius, height, segments=7)
        s.cone("snow", (mx, my, height - 5 - height * 0.18), radius * 0.38, height * 0.36, segments=7)
    return s


def main():
    args = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    if len(args) != 1:
        raise SystemExit("usage: ... -- <output .blend>")
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj)
    export = bpy.data.collections.new("Export")
    bpy.context.scene.collection.children.link(export)
    # One generator for every stage, seeded, so running this twice writes the same file and a change
    # to one stage does not reshuffle the windows of the others.
    names = []
    for build, seed in ((build_alley, 11), (build_temple, 23), (build_rooftop, 37), (build_shrine, 41)):
        stage = build(random.Random(seed))
        names.extend(sorted(stage.meshes))
        stage.build(export)
    bpy.ops.wm.save_as_mainfile(filepath=bpy.path.abspath(args[0]))
    print(f"Saved {args[0]} with {len(names)} parts")
    print("\n".join(names))


main()
