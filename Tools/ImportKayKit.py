# -*- coding: utf-8 -*-
import unreal
import os

SRC_ROOT = r"C:\Users\86178\Desktop\Ashen-Rift-The-Lost-Frontier\ThirdParty\KayKit-Dungeon\addons\kaykit_dungeon_remastered\Assets"
FBX_DIR = os.path.join(SRC_ROOT, "fbx")
TEX_PATH = os.path.join(SRC_ROOT, "texture", "dungeon_texture.png")
DEST_MESH = "/Game/AshenRift/KayKit/Meshes"
DEST_TEX = "/Game/AshenRift/KayKit/Textures"
DEST_MAT = "/Game/AshenRift/KayKit/Materials"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
editor_asset_lib = unreal.EditorAssetLibrary


def ensure_dir(path):
    if not editor_asset_lib.does_directory_exist(path):
        editor_asset_lib.make_directory(path)


def import_texture():
    ensure_dir(DEST_TEX)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", TEX_PATH)
    task.set_editor_property("destination_path", DEST_TEX)
    task.set_editor_property("destination_name", "T_DungeonAtlas")
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    asset_tools.import_asset_tasks([task])
    return DEST_TEX + "/T_DungeonAtlas"


def create_material(tex_asset_path):
    ensure_dir(DEST_MAT)
    mat_path = DEST_MAT + "/M_KayKitDungeon"
    if editor_asset_lib.does_asset_exist(mat_path):
        return unreal.load_asset(mat_path)
    mat_factory = unreal.MaterialFactoryNew()
    mat = asset_tools.create_asset("M_KayKitDungeon", DEST_MAT, unreal.Material, mat_factory)
    tex = unreal.load_asset(tex_asset_path)
    if mat and tex:
        # Sample texture into BaseColor via MaterialEditingLibrary
        try:
            tex_sample = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -350, 0)
            tex_sample.texture = tex
            unreal.MaterialEditingLibrary.connect_material_property(tex_sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
            unreal.MaterialEditingLibrary.recompile_material(mat)
        except Exception as e:
            unreal.log_warning("Material wiring failed: {}".format(e))
        editor_asset_lib.save_asset(mat_path)
    return mat


def import_fbx_batch(mat):
    ensure_dir(DEST_MESH)
    files = sorted([f for f in os.listdir(FBX_DIR) if f.lower().endswith(".fbx")])
    unreal.log("KayKit: importing {} FBX files".format(len(files)))
    batch = []
    for name in files:
        full = os.path.join(FBX_DIR, name)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", full)
        task.set_editor_property("destination_path", DEST_MESH)
        task.set_editor_property("destination_name", os.path.splitext(name)[0])
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
        sm.set_editor_property("import_uniform_scale", 100.0)  # KayKit metres -> UE cm
        task.set_editor_property("options", options)
        batch.append(task)
        if len(batch) >= 25:
            asset_tools.import_asset_tasks(batch)
            batch = []
    if batch:
        asset_tools.import_asset_tasks(batch)

    # Assign material to imported meshes
    if mat:
        assets = editor_asset_lib.list_assets(DEST_MESH, recursive=False, include_folder=False)
        for a in assets:
            mesh = unreal.load_asset(a)
            if isinstance(mesh, unreal.StaticMesh):
                try:
                    for i in range(mesh.get_num_sections(0)):
                        unreal.EditorStaticMeshLibrary.set_material(mesh, i, mat) if hasattr(unreal, "EditorStaticMeshLibrary") else None
                except Exception:
                    pass
                # Fallback: set via static mesh slots
                try:
                    mats = mesh.static_materials
                    if mats and len(mats) > 0:
                        slot = mats[0]
                        slot.set_editor_property("material_interface", mat)
                        mesh.set_editor_property("static_materials", [slot])
                    editor_asset_lib.save_asset(a)
                except Exception as e:
                    unreal.log_warning("Mat assign {}: {}".format(a, e))


def main():
    unreal.log("KayKit import start")
    tex_path = import_texture()
    mat = create_material(tex_path)
    import_fbx_batch(mat)
    unreal.log("KayKit import done -> {}".format(DEST_MESH))


main()
