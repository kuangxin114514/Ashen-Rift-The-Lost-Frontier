# -*- coding: utf-8 -*-
"""Import the KayKit wooden door leaf that ships only as OBJ (not FBX).

Source: ThirdParty/.../Assets/obj/wall_doorway_door.obj
Dest:   /Game/AshenRift/KayKit/Architecture/Doors/wall_doorway_door

Also imports wall_doorway_scaffold_door. Applies M_KayKitDungeon and
UseComplexAsSimple so an open doorway frame stays walkable when this leaf
is used as decor (prefer NoCollision on the placed leaf if it blocks the gap).

Run in UE 4.27: File > Execute Python Script > Tools/ImportKayKitDoor.py
"""
import unreal
import os

SRC_OBJ = r"C:\Users\86178\Desktop\Ashen-Rift-The-Lost-Frontier\ThirdParty\KayKit-Dungeon\addons\kaykit_dungeon_remastered\Assets\obj"
DEST = "/Game/AshenRift/KayKit/Architecture/Doors"
MAT = "/Game/AshenRift/KayKit/Materials/M_KayKitDungeon"
FILES = ("wall_doorway_door.obj", "wall_doorway_scaffold_door.obj")

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def ensure_dir(path):
    if not eal.does_directory_exist(path):
        eal.make_directory(path)


def import_obj(filename):
    ensure_dir(DEST)
    base = os.path.splitext(filename)[0]
    dest = DEST + "/" + base
    full = os.path.join(SRC_OBJ, filename)
    if not os.path.isfile(full):
        unreal.log_error("Missing door OBJ: " + full)
        return None
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", full)
    task.set_editor_property("destination_path", DEST)
    task.set_editor_property("destination_name", base)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_animations", False)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("automated_import_should_detect_type", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    sm = options.get_editor_property("static_mesh_import_data")
    sm.set_editor_property("combine_meshes", True)
    sm.set_editor_property("auto_generate_collision", True)
    # KayKit OBJ is metres; match FBX import scale used by ImportKayKit.py.
    sm.set_editor_property("import_uniform_scale", 100.0)
    task.set_editor_property("options", options)
    asset_tools.import_asset_tasks([task])
    if not eal.does_asset_exist(dest):
        unreal.log_error("Import failed: " + dest)
        return None
    return dest


def assign_material_and_collision(path):
    mesh = unreal.load_asset(path)
    if not mesh or not isinstance(mesh, unreal.StaticMesh):
        return
    mat = unreal.load_asset(MAT) if eal.does_asset_exist(MAT) else None
    if mat:
        try:
            slots = mesh.get_editor_property("static_materials")
            new_slots = []
            for slot in slots or []:
                slot.set_editor_property("material_interface", mat)
                new_slots.append(slot)
            if new_slots:
                mesh.set_editor_property("static_materials", new_slots)
        except Exception as exc:
            unreal.log_warning("mat {}: {}".format(path, exc))
    body = mesh.get_editor_property("body_setup")
    if body:
        body.set_editor_property(
            "collision_trace_flag",
            unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE,
        )
    eal.save_asset(path, only_if_is_dirty=False)
    unreal.log("KayKit door ready: " + path)


def main():
    unreal.log("KayKit door import start")
    for name in FILES:
        path = import_obj(name)
        if path:
            assign_material_and_collision(path)
    unreal.log("ASHEN_RIFT_KAYKIT_DOOR_IMPORTED")


main()
