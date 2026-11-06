# -*- coding: utf-8 -*-
import unreal

PATTERNS = ("door", "open", "gated", "window_open", "archedwindow_open", "broken")
ROOTS = [
    "/Game/AshenRift/KayKit/Architecture/Walls",
    "/Game/AshenRift/KayKit/Architecture/Stairs",
    "/Game/AshenRift/KayKit/Architecture/Barriers",
]


def should_fix(name):
    lower = name.lower()
    return any(p in lower for p in PATTERNS)


def fix_mesh(path):
    mesh = unreal.load_asset(path)
    if not mesh or not isinstance(mesh, unreal.StaticMesh):
        return False
    try:
        lib = unreal.EditorStaticMeshLibrary
        if hasattr(lib, "remove_collisions"):
            lib.remove_collisions(mesh)
    except Exception as e:
        unreal.log_warning("remove_collisions {}: {}".format(path, e))

    body = mesh.get_editor_property("body_setup")
    if body:
        body.set_editor_property(
            "collision_trace_flag",
            unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE,
        )
    unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    return True


def main():
    fixed = []
    for root in ROOTS:
        if not unreal.EditorAssetLibrary.does_directory_exist(root):
            continue
        for a in unreal.EditorAssetLibrary.list_assets(root, recursive=True, include_folder=False):
            name = a.split("/")[-1].split(".")[0]
            if not should_fix(name):
                continue
            if fix_mesh(a):
                fixed.append(name)
                unreal.log("KayKit collision fixed: {}".format(name))
    unreal.log("KayKit doorway collision fixed count={}".format(len(fixed)))


main()
