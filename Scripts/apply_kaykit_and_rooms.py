# -*- coding: utf-8 -*-
"""One-shot: import door leaf, create room BPs, replace greybox shell with KayKit.

Run via UE4Editor-Cmd -ExecutePythonScript, or File > Execute Python Script.
"""
import unreal
import importlib.util
import os

ROOT = r"C:\Users\86178\Desktop\Ashen-Rift-The-Lost-Frontier"


def run_file(rel):
    path = os.path.join(ROOT, rel)
    unreal.log("=== Running " + rel + " ===")
    spec = importlib.util.spec_from_file_location("ashen_step", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)


run_file(os.path.join("Tools", "ImportKayKitDoor.py"))
run_file(os.path.join("Scripts", "create_room_blueprints.py"))
run_file(os.path.join("Scripts", "replace_dungeon_shell_kaykit.py"))
unreal.log("ASHEN_RIFT_KAYKIT_AND_ROOMS_DONE")
