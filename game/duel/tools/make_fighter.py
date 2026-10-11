# Builds `src/model/fighter.blend`: a jointed mannequin of boxes with the handful of clips the game
# plays. Run by hand, by Blender itself, whenever the figure or a clip is to change:
#
#   blender --background --factory-startup --python make_fighter.py -- ../src/model/fighter.blend
#
# A script rather than a file somebody modelled because this is a stand-in. It is here so that the
# animation code has a skeleton, a walk that travels and a punch with a moment of impact to work
# on, and it is meant to be thrown away for a character an artist made. Until then, a clip is a
# dozen lines below and changing one is a matter of changing numbers.
#
# The figure stands on the origin facing -Y, which is the front in Blender and comes out of the
# glTF export as +Z. `Root` is a bone on the ground that nothing is skinned to: it is what the
# clips move to travel, and what the engine is told to take that travel from.
import math
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

FPS = 30
# name, parent, head, tail, box width and depth (nought for a bone with no box)
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


def build_armature():
    data = bpy.data.armatures.new("Fighter")
    armature = bpy.data.objects.new("Fighter", data)
    bpy.context.scene.collection.objects.link(armature)
    bpy.context.view_layer.objects.active = armature
    bpy.ops.object.mode_set(mode="EDIT")
    for name, parent, head, tail, width, depth in BONES:
        bone = data.edit_bones.new(name)
        bone.head, bone.tail = head, tail
        bone.roll = 0
        bone.use_deform = width > 0
        if parent:
            bone.parent = data.edit_bones[parent]
    bpy.ops.object.mode_set(mode="OBJECT")
    return armature


def build_mesh(armature):
    mesh = bpy.data.meshes.new("FighterBody")
    body = bpy.data.objects.new("FighterBody", mesh)
    bpy.context.scene.collection.objects.link(body)
    bm = bmesh.new()
    deform = bm.verts.layers.deform.verify()
    for name, _, head, tail, width, depth in BONES:
        if not width:
            continue
        group = body.vertex_groups.new(name=name)
        head, tail = Vector(head), Vector(tail)
        centre = (head + tail) / 2
        made = bmesh.ops.create_cube(bm, size=1, matrix=Matrix.Translation(centre) @ Matrix.Diagonal((width, depth, abs(tail.z - head.z) * 0.96, 1)))
        for vert in made["verts"]:
            vert[deform][group.index] = 1
    bm.normal_update()
    bm.to_mesh(mesh)
    bm.free()
    material = bpy.data.materials.new("FighterBody")
    shader = next(n for n in material.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    shader.inputs["Base Color"].default_value = (0.62, 0.6, 0.56, 1)
    shader.inputs["Roughness"].default_value = 0.6
    mesh.materials.append(material)
    body.parent = armature
    body.modifiers.new("Armature", "ARMATURE").object = armature
    return body


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


def main():
    args = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    if len(args) != 1:
        raise SystemExit("usage: ... -- <output .blend>")
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj)
    bpy.context.scene.render.fps = FPS
    armature = build_armature()
    body = build_mesh(armature)
    build_clips(armature)
    export = bpy.data.collections.new("Export")
    bpy.context.scene.collection.children.link(export)
    for obj in (armature, body):
        bpy.context.scene.collection.objects.unlink(obj)
        export.objects.link(obj)
    bpy.ops.wm.save_as_mainfile(filepath=bpy.path.abspath(args[0]))
    print(f"Saved {args[0]} with clips {', '.join(CLIPS)}")


main()
