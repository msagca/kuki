# Builds `src/model/fighter.blend`: the roster of fighters a player picks from, each a low-poly figure
# on one shared rig, with the handful of clips the game plays. Run by hand, by Blender itself,
# whenever a figure or a clip is to change:
#
#   blender --background --factory-startup --python make_fighter.py -- ../src/model/fighter.blend
#
# A script rather than a file somebody modelled because these are stand-ins. They are here so that
# the animation code has a skeleton, a walk that travels and a punch with a moment of impact to work
# on, and a player has someone to choose, and they are meant to be thrown away for characters an
# artist made. Until then, a clip is a dozen lines below and a hat is three.
#
# Every fighter has the same skeleton and the same clips -- the clips are made once and handed to
# every rig -- and only the mesh over the bones differs. That is what lets any two of them fight:
# the rules time every move from the clips, so fighters that shared none would be playing different
# games. Each rig is exported to a file of its own, named after it: `aoi.glb` and so on.
#
# The figure stands on the origin facing -Y, which is the front in Blender and comes out of the
# glTF export as +Z. `Root` is a bone on the ground that nothing is skinned to: it is what the
# clips move to travel, and what the engine is told to take that travel from.
import math
import sys

import bmesh
import bpy
from mathutils import Euler, Matrix, Vector

FPS = 30
# name, parent, head, tail, and whether anything is skinned to it (a width of nought for none)
BONES = [
    ("Root", None, (0, 0, 0), (0, 0, 0.2), 0, 0),
    ("Hips", "Root", (0, 0, 1.0), (0, 0, 1.2), 0.36, 0.22),
    ("Spine", "Hips", (0, 0, 1.2), (0, 0, 1.6), 0.4, 0.24),
    ("Head", "Spine", (0, 0, 1.65), (0, 0, 1.9), 0.22, 0.22),
    ("UpperArm.L", "Spine", (0.27, 0, 1.56), (0.27, 0, 1.26), 0.11, 0.11),
    ("ForeArm.L", "UpperArm.L", (0.27, 0, 1.26), (0.27, 0, 0.94), 0.1, 0.1),
    ("UpperArm.R", "Spine", (-0.27, 0, 1.56), (-0.27, 0, 1.26), 0.11, 0.11),
    ("ForeArm.R", "UpperArm.R", (-0.27, 0, 1.26), (-0.27, 0, 0.94), 0.1, 0.1),
    ("Thigh.L", "Hips", (0.1, 0, 1.0), (0.1, 0, 0.54), 0.15, 0.15),
    ("Shin.L", "Thigh.L", (0.1, 0, 0.54), (0.1, 0, 0.06), 0.12, 0.12),
    ("Thigh.R", "Hips", (-0.1, 0, 1.0), (-0.1, 0, 0.54), 0.15, 0.15),
    ("Shin.R", "Thigh.R", (-0.1, 0, 0.54), (-0.1, 0, 0.06), 0.12, 0.12),
]
# The stance everything starts from and comes back to: fists up, knees soft. Degrees a bone swings
# forward about the figure's side-to-side axis.
GUARD = {"UpperArm.L": 35, "ForeArm.L": 100, "UpperArm.R": 35, "ForeArm.R": 100, "Thigh.L": 8, "Shin.L": -12, "Thigh.R": 8, "Shin.R": -12}


def pose(**changes):
    result = dict(GUARD)
    result.update({name.replace("_", "."): value for name, value in changes.items()})
    return result


# Each clip: whether it loops, then (frame, forward swing by bone, twist of the spine about the
# upright, how far forward the root has travelled, how far the hips have dropped).
CLIPS = {
    "Idle": [
        (0, pose(), 0, 0, 0),
        (30, pose(UpperArm_L=39, UpperArm_R=39, Thigh_L=12, Shin_L=-20, Thigh_R=12, Shin_R=-20), 0, 0, 0.012),
        (60, pose(), 0, 0, 0),
    ],
    "Walk": [
        (0, pose(Thigh_L=32, Shin_L=-14, Thigh_R=-22, Shin_R=-30), 0, 0.0, 0.02),
        (8, pose(Thigh_L=6, Shin_L=-6, Thigh_R=10, Shin_R=-62), 0, 0.25, 0),
        (15, pose(Thigh_L=-22, Shin_L=-30, Thigh_R=32, Shin_R=-14), 0, 0.5, 0.02),
        (23, pose(Thigh_L=10, Shin_L=-62, Thigh_R=6, Shin_R=-6), 0, 0.75, 0),
        (30, pose(Thigh_L=32, Shin_L=-14, Thigh_R=-22, Shin_R=-30), 0, 1.0, 0.02),
    ],
    "Punch": [
        (0, pose(), 0, 0, 0),
        (4, pose(UpperArm_R=10, ForeArm_R=120), -18, 0, 0.02),
        (9, pose(UpperArm_R=92, ForeArm_R=4, Thigh_L=20, Shin_L=-24), 28, 0.16, 0.03),
        (13, pose(UpperArm_R=90, ForeArm_R=6, Thigh_L=20, Shin_L=-24), 26, 0.16, 0.03),
        (24, pose(), 0, 0.16, 0),
    ],
    "Kick": [
        (0, pose(), 0, 0, 0),
        (6, pose(Thigh_R=70, Shin_R=-95, Spine=-6), -10, 0, 0),
        (12, pose(Thigh_R=96, Shin_R=-4, Spine=-16), -14, 0.08, 0),
        (16, pose(Thigh_R=94, Shin_R=-6, Spine=-16), -14, 0.08, 0),
        (30, pose(), 0, 0.08, 0),
    ],
    "Hit": [
        (0, pose(), 0, 0, 0),
        (4, pose(Spine=-26, Head=-18, UpperArm_L=10, UpperArm_R=10, Thigh_L=-6, Thigh_R=-6), 8, -0.18, 0.02),
        (10, pose(Spine=-14, Head=-8), 4, -0.3, 0.03),
        (20, pose(), 0, -0.34, 0),
    ],
}


def build_armature(name):
    data = bpy.data.armatures.new(name)
    armature = bpy.data.objects.new(name, data)
    bpy.context.scene.collection.objects.link(armature)
    bpy.context.view_layer.objects.active = armature
    bpy.ops.object.mode_set(mode="EDIT")
    for bone_name, parent, head, tail, width, depth in BONES:
        bone = data.edit_bones.new(bone_name)
        bone.head, bone.tail = head, tail
        bone.roll = 0
        bone.use_deform = width > 0
        if parent:
            bone.parent = data.edit_bones[parent]
    bpy.ops.object.mode_set(mode="OBJECT")
    return armature


# The parts of a costume the game knows by name: it recolours the jacket, the trousers, the band,
# the gloves and the hair of a second player who picks the same fighter as the first. A material is
# called `<Fighter>_<Part>`, and the game reads the part after the last underscore.
PARTS = ("Skin", "Jacket", "Trousers", "Belt", "Band", "Hair", "Gloves", "Eyes")


class Figure:
    """A fighter's mesh: low-poly pieces, each rigid on one bone and in one material.

    Rigid because every piece is a part of a body that does not bend -- a forearm, a shin, a hat --
    and a joint is where two pieces meet. It is how a figure of a few hundred faces was built when
    that was all a machine could draw, and it reads, under the ink, as a drawing of one.
    """

    def __init__(self, name, colours):
        self.name = name
        self.colours = colours
        self.bm = bmesh.new()
        self.deform = self.bm.verts.layers.deform.verify()
        self.groups = []

    def _add(self, bone, material, made):
        if bone not in self.groups:
            self.groups.append(bone)
        group = self.groups.index(bone)
        for vert in made["verts"]:
            vert[self.deform][group] = 1
        for face in {f for v in made["verts"] for f in v.link_faces}:
            face.material_index = PARTS.index(material)

    def box(self, bone, material, centre, size, rotation=(0, 0, 0)):
        turn = Euler(tuple(math.radians(a) for a in rotation)).to_matrix().to_4x4()
        self._add(bone, material, bmesh.ops.create_cube(self.bm, size=1, matrix=Matrix.Translation(Vector(centre)) @ turn @ Matrix.Diagonal((*size, 1))))

    def prism(self, bone, material, start, end, radius, end_radius=None, sides=6, squash=(1, 1)):
        """A tapered prism from one point to another: a limb, a torso, a spike, a hat."""
        start, end = Vector(start), Vector(end)
        along = end - start
        rotation = along.to_track_quat("Z", "Y").to_matrix().to_4x4()
        matrix = Matrix.Translation((start + end) / 2) @ rotation @ Matrix.Diagonal((squash[0], squash[1], 1, 1))
        made = bmesh.ops.create_cone(
            self.bm,
            cap_ends=True,
            segments=sides,
            radius1=radius,
            radius2=radius if end_radius is None else end_radius,
            depth=along.length,
            matrix=matrix,
        )
        self._add(bone, material, made)

    def blob(self, bone, material, centre, radius, scale=(1, 1, 1), subdivisions=1):
        made = bmesh.ops.create_icosphere(self.bm, subdivisions=subdivisions, radius=radius, matrix=Matrix.Translation(Vector(centre)) @ Matrix.Diagonal((*scale, 1)))
        self._add(bone, material, made)

    def both(self, method, bone, *args, **kwargs):
        """The same piece on both sides, the right one mirrored across the figure's middle."""

        def mirror(value):
            return (-value[0], *value[1:]) if isinstance(value, tuple) and len(value) == 3 else value

        getattr(self, method)(bone + ".L", *args, **kwargs)
        getattr(self, method)(bone + ".R", *(mirror(a) for a in args), **{k: mirror(v) for k, v in kwargs.items()})

    def build(self, armature):
        mesh = bpy.data.meshes.new(self.name + "Body")
        body = bpy.data.objects.new(self.name + "Body", mesh)
        bpy.context.scene.collection.objects.link(body)
        for part in PARTS:
            material = bpy.data.materials.new(f"{self.name}_{part}")
            shader = next(n for n in material.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
            shader.inputs["Base Color"].default_value = (*self.colours[part], 1)
            shader.inputs["Roughness"].default_value = 0.6
            mesh.materials.append(material)
        self.bm.normal_update()
        self.bm.to_mesh(mesh)
        self.bm.free()
        for bone in self.groups:
            body.vertex_groups.new(name=bone)
        body.parent = armature
        body.modifiers.new("Armature", "ARMATURE").object = armature
        return body


def body(f, chest, waist, hip, arm, fore, fist, thigh, shin, foot, shoulder, jacket_arms=True, fists="Gloves", feet="Skin"):
    """The figure every fighter is built on: a pelvis, a torso, a neck, a head with eyes, and limbs
    tapering towards the hands and feet. What a fighter wears is laid over this."""
    f.prism("Hips", "Trousers", (0, 0, 0.95), (0, 0, 1.22), hip, waist, sides=8, squash=(1, 0.64))
    f.prism("Spine", "Jacket", (0, 0, 1.2), (0, 0, 1.6), waist, chest, sides=8, squash=(1, 0.6))
    f.prism("Spine", "Skin", (0, 0, 1.57), (0, 0, 1.7), 0.055, 0.05, sides=6)
    f.blob("Head", "Skin", (0, -0.005, 1.785), 0.115, (1.0, 1.05, 1.15), subdivisions=2)
    f.box("Head", "Eyes", (0.045, -0.118, 1.795), (0.034, 0.02, 0.024))
    f.box("Head", "Eyes", (-0.045, -0.118, 1.795), (0.034, 0.02, 0.024))
    f.both("blob", "UpperArm", "Jacket" if jacket_arms else "Skin", (0.25, 0, 1.53), shoulder, (1, 0.9, 0.9))
    f.both("prism", "UpperArm", "Jacket" if jacket_arms else "Skin", (0.27, 0, 1.54), (0.27, 0, 1.26), arm, arm * 0.85)
    f.both("prism", "ForeArm", "Skin", (0.27, 0, 1.27), (0.27, 0, 0.97), fore, fore * 0.78)
    f.both("box", "ForeArm", fists, (0.27, 0, 0.915), (fist, fist * 1.05, fist * 1.1))
    f.both("prism", "Thigh", "Trousers", (0.1, 0, 1.02), (0.1, 0, 0.54), thigh, thigh * 0.75)
    f.both("prism", "Shin", "Trousers", (0.1, 0, 0.55), (0.1, 0, 0.1), shin, shin * 0.72)
    f.both("box", "Shin", feet, (0.1, -0.045, 0.045), (foot * 0.5, foot, 0.09))


def hair_spikes(f, spikes, material="Hair"):
    for start, end, radius in spikes:
        f.prism("Head", material, start, end, radius, 0.0, sides=4)


def build_aoi():
    """The steady one: a short jacket over a belt, a headband, black hair swept up and back."""
    f = Figure("Aoi", {
        "Skin": (0.72, 0.45, 0.32), "Jacket": (0.06, 0.24, 0.5), "Trousers": (0.05, 0.06, 0.12),
        "Belt": (0.02, 0.02, 0.02), "Band": (0.85, 0.85, 0.82), "Hair": (0.025, 0.02, 0.02),
        "Gloves": (0.03, 0.03, 0.035), "Eyes": (0.01, 0.01, 0.015)})
    body(f, chest=0.215, waist=0.165, hip=0.18, arm=0.06, fore=0.05, fist=0.12, thigh=0.09, shin=0.068, foot=0.22, shoulder=0.085)
    # The jacket's skirt over the hips, open in a V at the chest, and tied with a belt.
    f.prism("Hips", "Jacket", (0, 0, 1.22), (0, 0, 1.02), 0.17, 0.195, sides=8, squash=(1, 0.66))
    f.box("Spine", "Skin", (0, -0.116, 1.52), (0.1, 0.02, 0.14), (0, 0, 0))
    f.prism("Hips", "Belt", (0, 0, 1.15), (0, 0, 1.2), 0.178, 0.178, sides=8, squash=(1, 0.68))
    f.box("Hips", "Belt", (0.06, -0.13, 1.09), (0.045, 0.02, 0.15), (0, 8, 0))
    f.box("Hips", "Belt", (0.1, -0.13, 1.07), (0.045, 0.02, 0.17), (0, -10, 0))
    # Hair: a cap and spikes swept up and back, a band round the brow with its ends flying.
    f.blob("Head", "Hair", (0, 0.02, 1.85), 0.122, (1.03, 1.05, 0.78), subdivisions=1)
    hair_spikes(f, [
        ((0.0, -0.04, 1.9), (0.02, 0.15, 2.02), 0.065),
        ((0.07, 0.0, 1.88), (0.16, 0.16, 1.96), 0.05),
        ((-0.07, 0.0, 1.88), (-0.16, 0.16, 1.96), 0.05),
        ((0.0, 0.08, 1.86), (0.0, 0.26, 1.89), 0.06),
        ((0.07, -0.08, 1.88), (0.1, -0.13, 1.95), 0.035),
        ((-0.05, -0.09, 1.88), (-0.08, -0.14, 1.93), 0.035),
    ])
    f.prism("Head", "Band", (0, 0, 1.82), (0, 0, 1.86), 0.122, 0.122, sides=8, squash=(1, 1.06))
    f.box("Head", "Band", (0.035, 0.18, 1.82), (0.03, 0.16, 0.02), (35, 0, 12))
    f.box("Head", "Band", (-0.02, 0.18, 1.815), (0.03, 0.16, 0.02), (40, 0, -10))
    return f


def build_kaede():
    """The quick one: slight, sleeveless, legs in guards, a long ponytail and a sash with tails."""
    f = Figure("Kaede", {
        "Skin": (0.8, 0.55, 0.42), "Jacket": (0.6, 0.05, 0.05), "Trousers": (0.04, 0.035, 0.05),
        "Belt": (0.9, 0.6, 0.08), "Band": (0.9, 0.88, 0.84), "Hair": (0.12, 0.05, 0.03),
        "Gloves": (0.55, 0.4, 0.08), "Eyes": (0.02, 0.01, 0.015)})
    body(f, chest=0.18, waist=0.13, hip=0.165, arm=0.045, fore=0.042, fist=0.1, thigh=0.08, shin=0.06, foot=0.2, shoulder=0.06, jacket_arms=False, feet="Trousers")
    # A high collar, the arms bound white from wrist to elbow, guards down the front of the shins.
    f.prism("Spine", "Jacket", (0, 0, 1.57), (0, 0, 1.64), 0.075, 0.065, sides=6)
    f.both("prism", "ForeArm", "Band", (0.27, 0, 1.2), (0.27, 0, 0.99), 0.05, 0.043)
    f.both("box", "Shin", "Gloves", (0.1, -0.05, 0.33), (0.1, 0.03, 0.38), (4, 0, 0))
    # A sash, its two ends hanging to the knee.
    f.prism("Hips", "Belt", (0, 0, 1.14), (0, 0, 1.21), 0.16, 0.16, sides=8, squash=(1, 0.66))
    f.box("Hips", "Belt", (0.15, -0.04, 0.92), (0.03, 0.06, 0.4), (0, 6, 0))
    f.box("Hips", "Belt", (0.17, 0.0, 0.95), (0.03, 0.06, 0.34), (0, -4, 0))
    # Hair: a cap with a fringe, and a ponytail tied high that falls down the back in segments.
    f.blob("Head", "Hair", (0, 0.015, 1.85), 0.122, (1.03, 1.05, 0.8), subdivisions=1)
    f.box("Head", "Hair", (0.0, -0.105, 1.87), (0.2, 0.03, 0.07), (-15, 0, 0))
    f.prism("Head", "Hair", (0.0, 0.1, 1.9), (0.0, 0.18, 1.74), 0.05, 0.045, sides=5)
    f.prism("Head", "Hair", (0.0, 0.18, 1.74), (0.0, 0.22, 1.52), 0.045, 0.035, sides=5)
    f.prism("Head", "Hair", (0.0, 0.22, 1.52), (0.0, 0.21, 1.36), 0.035, 0.0, sides=5)
    f.prism("Head", "Band", (0.0, 0.08, 1.89), (0.0, 0.12, 1.86), 0.055, 0.055, sides=6)
    return f


def build_goro():
    """The heavy one: a barrel of a chest, bare arms in iron gauntlets, a beard and a topknot."""
    f = Figure("Goro", {
        "Skin": (0.5, 0.3, 0.2), "Jacket": (0.18, 0.25, 0.08), "Trousers": (0.12, 0.09, 0.06),
        "Belt": (0.08, 0.05, 0.03), "Band": (0.75, 0.55, 0.1), "Hair": (0.03, 0.025, 0.02),
        "Gloves": (0.35, 0.35, 0.38), "Eyes": (0.01, 0.01, 0.01)})
    body(f, chest=0.29, waist=0.215, hip=0.2, arm=0.085, fore=0.078, fist=0.165, thigh=0.11, shin=0.088, foot=0.27, shoulder=0.12, jacket_arms=False, feet="Belt")
    # A vest open down the front, a broad belt with a gold buckle.
    f.box("Spine", "Skin", (0, -0.17, 1.42), (0.14, 0.03, 0.34))
    f.prism("Hips", "Belt", (0, 0, 1.12), (0, 0, 1.22), 0.205, 0.205, sides=8, squash=(1, 0.68))
    f.box("Hips", "Band", (0, -0.145, 1.17), (0.1, 0.02, 0.08))
    # Iron over the forearms and the fists, and cloth bound round the calves.
    f.both("prism", "ForeArm", "Gloves", (0.27, 0, 1.18), (0.27, 0, 0.98), 0.092, 0.088, sides=8)
    f.both("prism", "Shin", "Band", (0.1, 0, 0.38), (0.1, 0, 0.16), 0.085, 0.07, sides=6)
    # Bald, with a beard square under the jaw and a knot of hair on top.
    f.blob("Head", "Hair", (0, -0.07, 1.705), 0.085, (1.15, 0.75, 0.95), subdivisions=1)
    f.box("Head", "Hair", (0, -0.112, 1.835), (0.15, 0.025, 0.025), (0, 0, 0))
    f.blob("Head", "Hair", (0, 0.03, 1.93), 0.045, (1, 1, 0.8), subdivisions=1)
    f.prism("Head", "Hair", (0, 0.03, 1.95), (0, 0.06, 2.03), 0.025, 0.0, sides=5)
    f.prism("Head", "Band", (0, 0.03, 1.93), (0, 0.03, 1.95), 0.03, 0.03, sides=6)
    return f


def build_rin():
    """The quiet one: a wide straw hat, a scarf over the mouth, robe flaps that swing with the legs."""
    f = Figure("Rin", {
        "Skin": (0.75, 0.55, 0.45), "Jacket": (0.2, 0.06, 0.3), "Trousers": (0.03, 0.03, 0.04),
        "Belt": (0.5, 0.05, 0.06), "Band": (0.55, 0.04, 0.06), "Hair": (0.62, 0.5, 0.28),
        "Gloves": (0.02, 0.02, 0.02), "Eyes": (0.01, 0.01, 0.015)})
    body(f, chest=0.2, waist=0.15, hip=0.17, arm=0.055, fore=0.048, fist=0.11, thigh=0.085, shin=0.062, foot=0.21, shoulder=0.075, feet="Trousers")
    # A robe crossed at the chest, belted, its flaps hanging from the thighs front and back so that
    # they lift with a kick.
    f.box("Spine", "Belt", (0.03, -0.115, 1.42), (0.05, 0.02, 0.32), (0, -22, 0))
    f.prism("Hips", "Belt", (0, 0, 1.13), (0, 0, 1.2), 0.17, 0.17, sides=8, squash=(1, 0.66))
    f.both("box", "Thigh", "Jacket", (0.1, -0.085, 0.8), (0.16, 0.02, 0.48), (-4, 0, 0))
    f.both("box", "Thigh", "Jacket", (0.1, 0.085, 0.8), (0.16, 0.02, 0.48), (4, 0, 0))
    f.both("prism", "ForeArm", "Gloves", (0.27, 0, 1.12), (0.27, 0, 0.98), 0.052, 0.046)
    # The scarf over the nose and mouth, its end blowing out behind; the hat over everything.
    f.prism("Head", "Band", (0, -0.005, 1.68), (0, -0.005, 1.775), 0.125, 0.122, sides=8, squash=(1, 1.08))
    f.prism("Spine", "Band", (0.04, 0.08, 1.64), (0.1, 0.38, 1.46), 0.045, 0.02, sides=4)
    f.blob("Head", "Gloves", (0, 0.02, 1.86), 0.12, (1.02, 1.05, 0.7), subdivisions=1)
    f.prism("Head", "Hair", (0, 0, 1.9), (0, 0, 2.06), 0.33, 0.0, sides=10)
    f.prism("Head", "Belt", (0, 0, 1.895), (0, 0, 1.915), 0.125, 0.125, sides=10)
    return f


ROSTER = (build_aoi, build_kaede, build_goro, build_rin)


def about(bone, axis, degrees):
    # A turn about one of the figure's own axes, said in the bone's. A bone's pose is kept in the
    # frame it rests in, and which way that frame's axes point depends on where the bone's tail is
    # and on its roll; carrying the turn into it means no clip has to know.
    rest = bone.bone.matrix_local.to_3x3()
    return (rest.inverted() @ Matrix.Rotation(math.radians(degrees), 3, axis) @ rest).to_quaternion()


def build_clips(armature):
    animation = armature.animation_data_create()
    for bone in armature.pose.bones:
        bone.rotation_mode = "QUATERNION"
    root, hips, spine = (armature.pose.bones[name] for name in ("Root", "Hips", "Spine"))
    for name, keys in CLIPS.items():
        action = bpy.data.actions.new(name)
        animation.action = action
        for frame, swings, twist, travel, drop in keys:
            for bone in armature.pose.bones:
                # Forward is -Y, and a positive turn about X carries a hanging limb towards +Y.
                bone.rotation_quaternion = about(bone, "X", -swings.get(bone.name, 0))
            spine.rotation_quaternion = about(spine, "Z", twist) @ spine.rotation_quaternion
            root.location = root.bone.matrix_local.to_3x3().inverted() @ Vector((0, -travel, 0))
            hips.location = hips.bone.matrix_local.to_3x3().inverted() @ Vector((0, 0, -drop))
            for bone in armature.pose.bones:
                bone.keyframe_insert("rotation_quaternion", frame=frame)
            root.keyframe_insert("location", frame=frame)
            hips.keyframe_insert("location", frame=frame)
        # Straight lines between keys. The walk's travel has to be even or the feet slide, and the
        # rest are short enough that the keys are the shape.
        for layer in action.layers:
            for strip in layer.strips:
                for bag in strip.channelbags:
                    for curve in bag.fcurves:
                        for point in curve.keyframe_points:
                            point.interpolation = "LINEAR"
        # Stashed on a track of its own, which is how the glTF exporter comes to see every action
        # and not only the one left assigned.
        slot = animation.action_slot
        animation.action = None
        strip = animation.nla_tracks.new().strips.new(name, 0, action)
        strip.action_slot = slot
        strip.name = name
    for bone in armature.pose.bones:
        bone.rotation_quaternion = (1, 0, 0, 0)
        bone.location = (0, 0, 0)


def share_clips(source, target):
    """Hands the clips made on one rig to another with the same bones, track for track."""
    animation = target.animation_data_create()
    for bone in target.pose.bones:
        bone.rotation_mode = "QUATERNION"
    for track in source.animation_data.nla_tracks:
        for strip in track.strips:
            shared = animation.nla_tracks.new().strips.new(strip.name, int(strip.frame_start), strip.action)
            shared.action_slot = strip.action_slot
            shared.name = strip.name


def main():
    args = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    if len(args) != 1:
        raise SystemExit("usage: ... -- <output .blend>")
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj)
    bpy.context.scene.render.fps = FPS
    export = bpy.data.collections.new("Export")
    bpy.context.scene.collection.children.link(export)
    first = None
    for x, build in enumerate(ROSTER):
        figure = build()
        armature = build_armature(figure.name)
        body_object = figure.build(armature)
        if first is None:
            build_clips(armature)
            first = armature
        else:
            share_clips(first, armature)
        # Side by side in the file, to be looked at together; the export puts each back on the origin.
        armature.location.x = x * 1.2
        for obj in (armature, body_object):
            bpy.context.scene.collection.objects.unlink(obj)
            export.objects.link(obj)
    bpy.ops.wm.save_as_mainfile(filepath=bpy.path.abspath(args[0]))
    print(f"Saved {args[0]} with fighters {', '.join(b.__name__[6:] for b in ROSTER)} and clips {', '.join(CLIPS)}")


main()
