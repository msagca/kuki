# Exports every mesh and every rig in a .blend's `Export` collection to its own binary glTF, for
# `kuki_add_blender_models` in the root CMakeLists.txt. Run by Blender itself, headless:
#
#   blender --background --factory-startup <file.blend> --python-exit-code 1 \
#           --python export_blend_gltf.py -- <output directory> <stamp file>
#
# A collection rather than every mesh in the file, so a .blend can carry reference objects,
# helpers and layout scenes next to what ships. Each file is named after its object in snake case
# -- `ClockBody` becomes `clock_body.glb` -- which is the name the game looks the mesh up by.
#
# An object's transform is where it sits in the file to be looked at beside the others, not part
# of the model: the models are built in their own space, standing on their origins, and are
# exported as if the object were at the origin. The file is never saved, so nothing is moved.
#
# Binary glTF, one mesh to a file, Y up, with normals and UVs and nothing else. No materials,
# because the game draws every model with materials of its own; no animation, because nothing here
# is animated in the file -- what moves in the game, the clock's rocker, is moved by code.
#
# A rig is the exception to all of that. An armature in the collection is exported as one file
# with the meshes parented to it, their skinning, their materials, and every action stashed on its
# NLA tracks as a clip named after the action. Its meshes are part of that file and get none of
# their own.
import os
import re
import sys

import bpy
from mathutils import Matrix

COLLECTION = "Export"


def snake_case(name: str) -> str:
    return re.sub(r"(?<=[a-z0-9])(?=[A-Z])", "_", name).lower()


def main() -> None:
    args = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    if len(args) != 2:
        raise SystemExit("usage: ... -- <output directory> <stamp file>")
    output, stamp = args
    collection = bpy.data.collections.get(COLLECTION)
    if collection is None:
        raise SystemExit(f"{bpy.data.filepath} has no collection named {COLLECTION!r}")
    os.makedirs(output, exist_ok=True)
    # Cleared first, so a model taken out of the file stops being shipped rather than lingering as
    # the export of a mesh that no longer exists.
    for entry in os.listdir(output):
        if entry.endswith(".glb"):
            os.remove(os.path.join(output, entry))
    rigs = sorted((o for o in collection.all_objects if o.type == "ARMATURE"), key=lambda o: o.name)
    objects = sorted(
        (o for o in collection.all_objects if o.type == "MESH" and (o.parent is None or o.parent.type != "ARMATURE")),
        key=lambda o: o.name,
    )
    if not objects and not rigs:
        raise SystemExit(f"{bpy.data.filepath}: collection {COLLECTION!r} holds no meshes")
    # Over a copy: showing an object invalidates the collection's cached list of them, and an
    # iterator still walking that list goes on to hand out None.
    for obj in list(collection.all_objects):
        obj.hide_set(False)
        obj.hide_viewport = False
    names = set()
    for rig in rigs:
        name = snake_case(rig.name)
        names.add(name)
        bpy.ops.object.select_all(action="DESELECT")
        rig.select_set(True)
        for child in rig.children_recursive:
            child.select_set(True)
        bpy.context.view_layer.objects.active = rig
        # Every bone and every key, with nothing tidied away. The exporter's economies drop a bone
        # that deforms nothing and a channel that never changes, and the first of those is the root
        # the game takes the figure's travel from.
        bpy.ops.export_scene.gltf(
            filepath=os.path.join(output, name + ".glb"),
            export_format="GLB",
            use_selection=True,
            export_yup=True,
            export_materials="EXPORT",
            export_normals=True,
            export_texcoords=True,
            export_skins=True,
            export_def_bones=False,
            export_animations=True,
            export_animation_mode="ACTIONS",
            export_force_sampling=True,
            export_optimize_animation_size=False,
            export_optimize_animation_keep_anim_armature=True,
        )
    for obj in objects:
        name = snake_case(obj.name)
        if name in names:
            raise SystemExit(f"two objects in {COLLECTION!r} export to {name}.glb")
        names.add(name)
        placed = obj.matrix_world.copy()
        obj.matrix_world = Matrix.Identity(4)
        bpy.ops.object.select_all(action="DESELECT")
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        bpy.ops.export_scene.gltf(
            filepath=os.path.join(output, name + ".glb"),
            export_format="GLB",
            use_selection=True,
            export_yup=True,
            export_apply=True,
            export_materials="NONE",
            export_normals=True,
            export_texcoords=True,
            export_animations=False,
        )
        obj.matrix_world = placed
    with open(stamp, "w", encoding="utf-8") as f:
        f.write("\n".join(sorted(names)) + "\n")
    print(f"Exported {len(names)} models from {bpy.data.filepath} to {output}")


main()
