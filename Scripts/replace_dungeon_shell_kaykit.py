# -*- coding: utf-8 -*-
"""Replace L_RandomDungeon greybox SideWall / DoorWall / floors with KayKit meshes.

Also places the extracted wooden door leaf (wall_doorway_door) swung open beside
each doorway with NoCollision so the opening stays walkable.

Greybox mapping:
  SideWall / Camp_Back / Dungeon_End  -> Architecture/Walls/wall
  DoorWall partitions                 -> wall + wall_doorway (opening)
  Floor_*                             -> Architecture/Floors/floor_tile_large
  (new) DoorLeaf_*                    -> Architecture/Doors/wall_doorway_door

Run AFTER Tools/ImportKayKitDoor.py (once) so the door leaf exists.
Safe-ish to re-run: deletes prior KayKitShell_* / DoorLeaf_* labels first,
then removes remaining greybox SideWall/DoorWall/Floor/Camp_Back/Dungeon_End.

  File > Execute Python Script > Scripts/replace_dungeon_shell_kaykit.py
"""
import unreal

ROOT = "/Game/AshenRift"
MAP = ROOT + "/Maps/L_RandomDungeon"
WALL = ROOT + "/KayKit/Architecture/Walls/wall"
DOORWAY = ROOT + "/KayKit/Architecture/Walls/wall_doorway"
DOOR = ROOT + "/KayKit/Architecture/Doors/wall_doorway_door"
FLOOR = ROOT + "/KayKit/Architecture/Floors/floor_tile_large"

lib = unreal.EditorAssetLibrary
level = unreal.EditorLevelLibrary

ROOM_W = 2400.0
ROOM_D = 2000.0
TILE = 400.0
NUM_ROOMS = 6


def load_mesh(path):
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError("Missing mesh: " + path)
    return asset


def spawn_mesh(label, mesh, location, rotation=None, scale=None, no_collision=False):
    actor = level.spawn_actor_from_class(
        unreal.StaticMeshActor,
        unreal.Vector(*location),
        unreal.Rotator(*(rotation or (0, 0, 0))),
    )
    actor.set_actor_label(label)
    smc = actor.static_mesh_component
    smc.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    smc.set_static_mesh(mesh)
    if scale:
        actor.set_actor_scale3d(unreal.Vector(*scale))
    if no_collision:
        smc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    else:
        smc.set_collision_profile_name("BlockAll")
        smc.set_editor_property("can_ever_affect_navigation", True)
    return actor


def destroy_labelled(prefixes):
    removed = 0
    for actor in list(level.get_all_level_actors()):
        try:
            label = actor.get_actor_label()
        except Exception:
            continue
        if any(label == p or label.startswith(p) for p in prefixes):
            level.destroy_actor(actor)
            removed += 1
    return removed


def build_floors(floor_mesh):
    # 6 x 5 tiles cover 2400 x 2000 per room slot.
    count = 0
    for room in range(NUM_ROOMS):
        base_x = room * ROOM_W
        for ix in range(6):
            for iy in range(5):
                x = base_x - ROOM_W * 0.5 + TILE * 0.5 + ix * TILE
                y = -ROOM_D * 0.5 + TILE * 0.5 + iy * TILE
                spawn_mesh("KayKitShell_Floor_%d_%d_%d" % (room, ix, iy), floor_mesh, (x, y, 0))
                count += 1
    return count


def build_side_walls(wall_mesh):
    # Long corridor walls at y = ±1000. Wall piece is 400cm along local X.
    count = 0
    for room in range(NUM_ROOMS):
        base_x = room * ROOM_W
        for side, yaw in ((-1, 180.0), (1, 0.0)):
            y = side * 1000.0
            for i in range(6):
                x = base_x - ROOM_W * 0.5 + TILE * 0.5 + i * TILE
                spawn_mesh("KayKitShell_SideWall_%d_%d_%d" % (room, side, i), wall_mesh, (x, y, 0), (0, yaw, 0))
                count += 1
    return count


def build_end_walls(wall_mesh):
    count = 0
    # Camp_Back was at -1200; Dungeon_End at 13200 (create_random_dungeon.py).
    for x, tag in ((-1200.0, "CampBack"), (13200.0, "DungeonEnd")):
        for i, y in enumerate((-800.0, -400.0, 0.0, 400.0, 800.0)):
            spawn_mesh("KayKitShell_%s_%d" % (tag, i), wall_mesh, (x, y, 0), (0, 90.0, 0))
            count += 1
    return count


def build_door_partitions(wall_mesh, doorway_mesh, door_mesh):
    """Between rooms: solid walls on the sides + wall_doorway in the center."""
    count = 0
    for room in range(NUM_ROOMS - 1):
        x = room * ROOM_W + 1200.0
        # Side fills along Y (leave center ±200 for doorway piece).
        for y in (-800.0, -400.0, 400.0, 800.0):
            spawn_mesh("KayKitShell_DoorWall_%d_%.0f" % (room, y), wall_mesh, (x, y, 0), (0, 90.0, 0))
            count += 1
        # Arched stone doorway frame (walkable opening via complex-as-simple).
        spawn_mesh("KayKitShell_Doorway_%d" % room, doorway_mesh, (x, 0.0, 0), (0, 90.0, 0))
        count += 1
        # Wooden door leaf swung open beside the opening — visual only.
        if door_mesh:
            spawn_mesh(
                "DoorLeaf_%d" % room,
                door_mesh,
                (x + 40.0, 140.0, 0),
                (0, 20.0, 0),
                no_collision=True,
            )
            count += 1
    return count


def main():
    unreal.log("KayKit shell replace start")
    if not level.load_level(MAP):
        raise RuntimeError("Could not load " + MAP)

    wall = load_mesh(WALL)
    doorway = load_mesh(DOORWAY)
    floor = load_mesh(FLOOR)
    door = load_mesh(DOOR) if lib.does_asset_exist(DOOR) else None
    if not door:
        unreal.log_warning("Door leaf missing; run Tools/ImportKayKitDoor.py first. Continuing without DoorLeaf actors.")

    removed = destroy_labelled([
        "KayKitShell_",
        "DoorLeaf_",
        "SideWall",
        "DoorWall",
        "Floor_",
        "Camp_Back",
        "Dungeon_End",
    ])
    unreal.log("Removed %d prior shell actors" % removed)

    n = 0
    n += build_floors(floor)
    n += build_side_walls(wall)
    n += build_end_walls(wall)
    n += build_door_partitions(wall, doorway, door)

    assert level.save_current_level()
    unreal.log("KayKit shell actors placed=%d" % n)
    unreal.log("ASHEN_RIFT_KAYKIT_SHELL_APPLIED")


main()

