# -*- coding: utf-8 -*-
import unreal
import os
import re

SRC_FBX = r"C:\Users\86178\Desktop\Ashen-Rift-The-Lost-Frontier\ThirdParty\KayKit_Dungeon_Pack_1.1_FREE\Assets\fbx"
SRC_TEX = r"C:\Users\86178\Desktop\Ashen-Rift-The-Lost-Frontier\ThirdParty\KayKit_Dungeon_Pack_1.1_FREE\Assets\textures\dungeon_texture.png"
BASE = "/Game/AshenRift/KayKit"
MESH_FLAT = BASE + "/Meshes"
TEX_DIR = BASE + "/Textures"
MAT_DIR = BASE + "/Materials"

# category folder under KayKit/
CATEGORIES = {
    "Architecture/Walls": re.compile(r"^(wall|wall_)", re.I),
    "Architecture/Floors": re.compile(r"^(floor|floor_)", re.I),
    "Architecture/Ceilings": re.compile(r"^(ceiling)", re.I),
    "Architecture/Stairs": re.compile(r"^(stairs)", re.I),
    "Architecture/Doors": re.compile(r"^(door)", re.I),
    "Architecture/Columns": re.compile(r"^(column|pillar)", re.I),
    "Architecture/Barriers": re.compile(r"^(barrier)", re.I),
    "Props/Furniture": re.compile(r"^(table|chair|stool|bed|shelf|shelves|trunk)", re.I),
    "Props/Containers": re.compile(r"^(barrel|box|chest|crate|keg)", re.I),
    "Props/Lights": re.compile(r"^(candle|torch)", re.I),
    "Props/Decor": re.compile(r"^(banner|bottle|plate|rubble)", re.I),
    "Props/Treasure": re.compile(r"^(coin|key|keyring|sword)", re.I),
}

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def ensure_dir(path):
    if not eal.does_directory_exist(path):
        eal.make_directory(path)


def category_for(name):
    for folder, pat in CATEGORIES.items():
        if pat.search(name):
            return folder
    return "Props/Misc"


def import_missing_fbx():
    ensure_dir(MESH_FLAT)
    existing = set()
    for a in eal.list_assets(BASE, recursive=True, include_folder=False):
        existing.add(a.split("/")[-1].split(".")[0])
    files = [f for f in os.listdir(SRC_FBX) if f.lower().endswith(".fbx")]
    batch = []
    for name in files:
        base = os.path.splitext(name)[0]
        if base in existing:
            continue
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", os.path.join(SRC_FBX, name))
        task.set_editor_property("destination_path", MESH_FLAT)
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
        sm.set_editor_property("import_uniform_scale", 100.0)
        task.set_editor_property("options", options)
        batch.append(task)
    unreal.log("KayKit: importing {} missing FBX".format(len(batch)))
    if batch:
        asset_tools.import_asset_tasks(batch)


def ensure_material():
    ensure_dir(TEX_DIR)
    ensure_dir(MAT_DIR)
    tex_asset = TEX_DIR + "/T_DungeonAtlas"
    if not eal.does_asset_exist(tex_asset) and os.path.isfile(SRC_TEX):
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", SRC_TEX)
        task.set_editor_property("destination_path", TEX_DIR)
        task.set_editor_property("destination_name", "T_DungeonAtlas")
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        asset_tools.import_asset_tasks([task])
    mat_path = MAT_DIR + "/M_KayKitDungeon"
    mat = unreal.load_asset(mat_path) if eal.does_asset_exist(mat_path) else None
    return mat


def move_all_meshes():
    # Collect all static meshes under KayKit (any depth)
    assets = eal.list_assets(BASE, recursive=True, include_folder=False)
    moved = 0
    for path in assets:
        name = path.split("/")[-1].split(".")[0]
        asset = unreal.load_asset(path)
        if not isinstance(asset, unreal.StaticMesh):
            continue
        folder = category_for(name)
        dest_dir = BASE + "/" + folder
        ensure_dir(dest_dir)
        dest = dest_dir + "/" + name
        if path.split(".")[0] == dest:
            continue
        # If already at dest, skip
        if eal.does_asset_exist(dest) and path.startswith(dest_dir):
            continue
        ok = eal.rename_asset(path, dest)
        if ok:
            moved += 1
        else:
            unreal.log_warning("Failed move {} -> {}".format(path, dest))
    unreal.log("KayKit: moved {} meshes into categories".format(moved))
    # Clean empty flat Meshes folder if empty
    remaining = eal.list_assets(MESH_FLAT, recursive=False, include_folder=False) if eal.does_directory_exist(MESH_FLAT) else []
    if not remaining and eal.does_directory_exist(MESH_FLAT):
        eal.delete_directory(MESH_FLAT)


def assign_material(mat):
    if not mat:
        return
    for path in eal.list_assets(BASE, recursive=True, include_folder=False):
        mesh = unreal.load_asset(path)
        if not isinstance(mesh, unreal.StaticMesh):
            continue
        try:
            mats = mesh.get_editor_property("static_materials")
            if mats is None:
                continue
            new_slots = []
            for slot in mats:
                slot.set_editor_property("material_interface", mat)
                new_slots.append(slot)
            if new_slots:
                mesh.set_editor_property("static_materials", new_slots)
                eal.save_asset(path)
        except Exception as e:
            unreal.log_warning("mat {}: {}".format(path, e))


def main():
    unreal.log("KayKit categorize start")
    import_missing_fbx()
    mat = ensure_material()
    move_all_meshes()
    assign_material(mat)
    # summary
    for folder in sorted(set(CATEGORIES.keys()) | {"Props/Misc"}):
        d = BASE + "/" + folder
        n = len(eal.list_assets(d, recursive=False, include_folder=False)) if eal.does_directory_exist(d) else 0
        unreal.log("KayKit cat {:30} {}".format(folder, n))
    unreal.log("KayKit categorize done")


main()
