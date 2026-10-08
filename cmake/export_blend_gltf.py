# Exports every mesh in a .blend's `Export` collection to its own binary glTF, for
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
    objects = sorted((o for o in collection.all_objects if o.type == "MESH"), key=lambda o: o.name)
    if not objects:
        raise SystemExit(f"{bpy.data.filepath}: collection {COLLECTION!r} holds no meshes")
    for obj in objects:
        obj.hide_set(False)
        obj.hide_viewport = False
    names = set()
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
