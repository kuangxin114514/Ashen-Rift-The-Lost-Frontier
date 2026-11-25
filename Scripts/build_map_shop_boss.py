# -*- coding: utf-8 -*-
"""Create Map_Shop and Map_Boss as brand-new empty KayKit levels.

Scenery only. No characters, AI, HUD, or GameMode override.
Does not touch Map_Dungeon1/2/3, Map_Hub, or L_* maps.
"""
from __future__ import print_function
import traceback
import unreal

LOG_PATH = r"C:\Users\86178\Desktop\Ashen-Rift-The-Lost-Frontier\Saved\Logs\build_map_shop_boss.txt"
MAP_DIR = "/Game/AshenRift/Maps"
SHOP_MAP = MAP_DIR + "/Map_Shop"
BOSS_MAP = MAP_DIR + "/Map_Boss"
MESH = "/Game/Environments/KayKitDungeon/Meshes"
TILE = 400.0

PROTECTED = (
    "Map_Dungeon1", "Map_Dungeon2", "Map_Dungeon3", "Map_Hub",
    "L_RandomDungeon", "L_FirstPlayable", "L_Prototype", "L_Progression",
    "L_OriginalGreybox_Backup_20261028",
)

_lines = []


def log(msg):
    s = str(msg)
    _lines.append(s)
    try:
        unreal.log("ShopBossMaps: " + s)
    except Exception:
        print("ShopBossMaps: " + s)


def flush():
    try:
        import os
        folder = os.path.dirname(LOG_PATH)
        if folder and not os.path.isdir(folder):
            os.makedirs(folder)
        with open(LOG_PATH, "w", encoding="utf-8") as f:
            f.write("\n".join(_lines) + "\n")
    except Exception as e:
        try:
            unreal.log_warning("flush log failed: " + str(e))
        except Exception:
            pass


def mesh_path(rel):
    return MESH + "/" + rel


def load_mesh(rel):
    path = mesh_path(rel)
    asset = unreal.load_asset(path)
    if not asset:
        log("MISSING mesh " + path)
    return asset


def bbox_size(mesh):
    try:
        bb = mesh.get_bounding_box()
        return (
            abs(bb.max.x - bb.min.x),
            abs(bb.max.y - bb.min.y),
            abs(bb.max.z - bb.min.z),
        )
    except Exception as e:
        log("bbox fail %s: %s" % (mesh, e))
        return (0.0, 0.0, 0.0)


def spawn_actor(cls, loc, rot=None):
    if rot is None:
        rot = unreal.Rotator(0.0, 0.0, 0.0)
    elif not isinstance(rot, unreal.Rotator):
        rot = unreal.Rotator(float(rot[0]), float(rot[1]), float(rot[2]))
    if not isinstance(loc, unreal.Vector):
        loc = unreal.Vector(float(loc[0]), float(loc[1]), float(loc[2]))
    try:
        sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        return sub.spawn_actor_from_class(cls, loc, rot)
    except Exception:
        return unreal.EditorLevelLibrary.spawn_actor_from_class(cls, loc, rot)


def spawn_mesh(label, mesh, loc, yaw=0.0, z=0.0, no_collision=False):
    if mesh is None:
        log("skip %s (no mesh)" % label)
        return None
    if isinstance(loc, (tuple, list)):
        if len(loc) == 2:
            loc = (loc[0], loc[1], z)
        location = unreal.Vector(float(loc[0]), float(loc[1]), float(loc[2]))
    else:
        location = loc
    actor = spawn_actor(unreal.StaticMeshActor, location, unreal.Rotator(0.0, float(yaw), 0.0))
    if not actor:
        log("FAIL spawn " + label)
        return None
    try:
        actor.set_actor_label(label)
    except Exception:
        pass
    smc = actor.static_mesh_component
    try:
        smc.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    except Exception:
        pass
    smc.set_static_mesh(mesh)
    if no_collision:
        smc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    else:
        try:
            smc.set_collision_profile_name("BlockAll")
        except Exception:
            pass
        try:
            smc.set_editor_property("can_ever_affect_navigation", True)
        except Exception:
            pass
    return actor


def set_point_light(actor, intensity, radius, color, temp=2800.0):
    comp = None
    for name in ("light_component", "point_light_component"):
        try:
            comp = actor.get_editor_property(name)
            if comp:
                break
        except Exception:
            pass
    if comp is None:
        try:
            comp = actor.light_component
        except Exception:
            log("no light component on " + str(actor))
            return
    try:
        comp.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    except Exception:
        pass
    try:
        comp.set_editor_property("intensity", float(intensity))
    except Exception as e:
        log("intensity: " + str(e))
    try:
        if not isinstance(color, unreal.LinearColor):
            color = unreal.LinearColor(float(color[0]), float(color[1]), float(color[2]), 1.0)
        comp.set_editor_property("light_color", color)
    except Exception as e:
        log("light_color: " + str(e))
    try:
        comp.set_editor_property("attenuation_radius", float(radius))
    except Exception:
        pass
    try:
        comp.set_editor_property("cast_shadows", True)
    except Exception:
        pass
    try:
        comp.set_editor_property("use_temperature", True)
        comp.set_editor_property("temperature", float(temp))
    except Exception:
        pass
    try:
        comp.set_editor_property("indirect_lighting_intensity", 0.0)
    except Exception:
        pass


def clear_gamemode():
    world = unreal.EditorLevelLibrary.get_editor_world()
    ws = world.get_world_settings()
    for prop in ("default_game_mode", "DefaultGameMode"):
        try:
            ws.set_editor_property(prop, None)
            log("cleared WorldSettings." + prop)
        except Exception as e:
            log("clear %s: %s" % (prop, e))
    for prop in ("default_game_mode", "DefaultGameMode"):
        try:
            val = ws.get_editor_property(prop)
            log("WorldSettings.%s = %s" % (prop, val))
        except Exception:
            pass


def current_world_path():
    try:
        return str(unreal.EditorLevelLibrary.get_editor_world().get_path_name())
    except Exception:
        return ""


def assert_not_protected(asset_path):
    name = asset_path.rstrip("/").split("/")[-1]
    if name in PROTECTED:
        raise RuntimeError("Refusing to write protected map: " + asset_path)


def create_empty_level(asset_path):
    assert_not_protected(asset_path)
    name = asset_path.rstrip("/").split("/")[-1]
    folder = asset_path[: asset_path.rfind("/")]
    if not unreal.EditorAssetLibrary.does_directory_exist(folder):
        unreal.EditorAssetLibrary.make_directory(folder)
        log("mkdir " + folder)
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        log("asset already exists, deleting so we can recreate as a new empty level: " + asset_path)
        try:
            unreal.EditorLevelLibrary.new_blank_map(False)
        except Exception:
            pass
        ok = unreal.EditorAssetLibrary.delete_asset(asset_path)
        log("delete_asset %s -> %s" % (asset_path, ok))
    try:
        unreal.EditorLevelLibrary.new_level(asset_path)
        log("new_level " + asset_path)
    except Exception as e:
        log("new_level failed: " + str(e))
        factory = unreal.WorldFactory()
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        world_asset = asset_tools.create_asset(name, folder, unreal.World, factory)
        if not world_asset:
            raise RuntimeError("Could not create " + asset_path)
        unreal.EditorAssetLibrary.save_loaded_asset(world_asset)
        unreal.EditorLoadingAndSavingUtils.load_map(asset_path)
        log("created via WorldFactory " + asset_path)
    path = current_world_path()
    log("current_world=" + path)
    if name not in path:
        raise RuntimeError("Editor did not open new level " + asset_path + " (opened " + path + ")")
    clear_gamemode()
    return True


def save_level(asset_path):
    clear_gamemode()
    ok = False
    try:
        ok = bool(unreal.EditorLevelLibrary.save_current_level())
    except Exception as e:
        log("save_current_level: " + str(e))
    try:
        unreal.EditorAssetLibrary.save_asset(asset_path)
    except Exception as e:
        log("save_asset: " + str(e))
    exists = unreal.EditorAssetLibrary.does_asset_exist(asset_path)
    log("saved %s ok=%s exists=%s" % (asset_path, ok, exists))
    return exists


# Wall yaw: nice face points into the room (same convention as replace_dungeon_shell_kaykit.py).
DIR = {
    "N": (0, 1, 0.0),
    "S": (0, -1, 180.0),
    "E": (1, 0, 90.0),
    "W": (-1, 0, -90.0),
}


class LevelBuilder(object):
    def __init__(self, prefix, meshes):
        self.prefix = prefix
        self.m = meshes
        self.n = 0
        self.cells = set()
        self.floors = {}
        self.edge_mesh = {}
        self.edge_open = set()

    def add_cells(self, cells, floor_key="floor"):
        for c in cells:
            self.cells.add(c)
            self.floors[c] = floor_key

    def open_edge(self, ix, iy, d):
        self.edge_open.add((ix, iy, d))

    def set_edge(self, ix, iy, d, kind):
        self.edge_mesh[(ix, iy, d)] = kind

    def _label(self, kind):
        self.n += 1
        return "%s_%s_%d" % (self.prefix, kind, self.n)

    def place_floors(self):
        for (ix, iy), key in sorted(self.floors.items()):
            mesh = self.m.get(key) or self.m["floor"]
            spawn_mesh(self._label("Floor"), mesh, (ix * TILE, iy * TILE, 0.0))

    def place_walls(self):
        for (ix, iy) in sorted(self.cells):
            cx, cy = ix * TILE, iy * TILE
            for d, (dx, dy, yaw) in DIR.items():
                nx, ny = ix + dx, iy + dy
                if (nx, ny) in self.cells:
                    continue
                if (ix, iy, d) in self.edge_open:
                    continue
                kind = self.edge_mesh.get((ix, iy, d), "wall")
                mesh = self.m.get(kind) or self.m["wall"]
                ex = cx + dx * (TILE * 0.5)
                ey = cy + dy * (TILE * 0.5)
                spawn_mesh(self._label(kind), mesh, (ex, ey, 0.0), yaw=yaw)

    def prop(self, key, loc, yaw=0.0, z=0.0, tag="Prop"):
        mesh = self.m.get(key)
        if mesh is None:
            log("missing prop key " + key)
            return None
        if isinstance(loc, (tuple, list)) and len(loc) == 2:
            loc = (loc[0], loc[1], z)
        return spawn_mesh(self._label(tag), mesh, loc, yaw=yaw)

    def torch(self, loc, yaw=0.0, z=150.0, intensity=4500.0, radius=1400.0, color=(1.0, 0.58, 0.28), temp=2700.0):
        if isinstance(loc, (tuple, list)) and len(loc) == 2:
            tloc = (loc[0], loc[1], z)
        else:
            tloc = loc
        self.prop("torch", tloc, yaw=yaw, tag="Torch")
        light = spawn_actor(
            unreal.PointLight,
            unreal.Vector(float(tloc[0]), float(tloc[1]), float(tloc[2]) + 30.0),
        )
        if light:
            try:
                light.set_actor_label(self._label("Light"))
            except Exception:
                pass
            set_point_light(light, intensity, radius, color, temp)
        return light


def add_common_lights(builder, warm=True):
    sky = spawn_actor(unreal.SkyLight, unreal.Vector(0.0, 0.0, 800.0))
    if sky:
        try:
            sky.set_actor_label(builder.prefix + "_SkyLight")
        except Exception:
            pass
        comp = None
        try:
            comp = sky.light_component
        except Exception:
            pass
        if comp:
            try:
                comp.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
            except Exception:
                pass
            try:
                comp.set_editor_property("intensity", 0.55 if warm else 0.4)
            except Exception:
                pass
            try:
                if warm:
                    comp.set_editor_property("light_color", unreal.LinearColor(1.0, 0.72, 0.48, 1.0))
            except Exception:
                pass
    sun = spawn_actor(
        unreal.DirectionalLight,
        unreal.Vector(0.0, 0.0, 900.0),
        unreal.Rotator(-50.0, -35.0, 0.0),
    )
    if sun:
        try:
            sun.set_actor_label(builder.prefix + "_FillSun")
        except Exception:
            pass
        try:
            comp = sun.light_component
            try:
                comp.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
            except Exception:
                pass
            try:
                comp.set_editor_property("intensity", 2.2 if warm else 1.4)
            except Exception:
                pass
            try:
                comp.set_editor_property("light_color", unreal.LinearColor(1.0, 0.7, 0.45, 1.0))
            except Exception:
                pass
            try:
                comp.set_editor_property("cast_shadows", True)
            except Exception:
                pass
        except Exception as e:
            log("sun light: " + str(e))
    try:
        fog = spawn_actor(unreal.ExponentialHeightFog, unreal.Vector(0.0, 0.0, 0.0))
        if fog:
            try:
                fog.set_actor_label(builder.prefix + "_Fog")
            except Exception:
                pass
            try:
                fc = fog.component if hasattr(fog, "component") else fog.height_fog_component
                fc.set_editor_property("fog_density", 0.018)
                fc.set_editor_property(
                    "fog_inscattering_color",
                    unreal.LinearColor(0.55, 0.28, 0.12, 1.0) if warm else unreal.LinearColor(0.45, 0.18, 0.08, 1.0),
                )
            except Exception as e:
                log("fog props: " + str(e))
    except Exception as e:
        log("no ExponentialHeightFog: " + str(e))


def add_player_start(label, loc, yaw):
    ps = spawn_actor(
        unreal.PlayerStart,
        unreal.Vector(float(loc[0]), float(loc[1]), 110.0),
        unreal.Rotator(0.0, float(yaw), 0.0),
    )
    if ps:
        try:
            ps.set_actor_label(label)
        except Exception:
            pass
        log("PlayerStart at %s yaw=%s" % (loc, yaw))
    else:
        log("FAIL PlayerStart")
    return ps


def load_all_meshes():
    rels = {
        "floor": "Floors/SM_floor_tile_large",
        "floor_rocks": "Floors/SM_floor_tile_large_rocks",
        "dirt": "Floors/SM_floor_dirt_large",
        "dirt_rocky": "Floors/SM_floor_dirt_large_rocky",
        "wall": "Walls/SM_wall",
        "wall_cracked": "Walls/SM_wall_cracked",
        "wall_scaffold": "Walls/SM_wall_scaffold",
        "wall_shelves": "Walls/SM_wall_shelves",
        "doorway": "Walls/SM_wall_doorway",
        "gated": "Walls/SM_wall_gated",
        "arched": "Walls/SM_wall_arched",
        "stairs": "Stairs/SM_stairs",
        "stairs_narrow": "Stairs/SM_stairs_narrow",
        "stairs_wide": "Stairs/SM_stairs_wide",
        "pillar": "Props/SM_pillar_decorated",
        "wall_pillar": "Walls/SM_wall_pillar",
        "table_long": "Props/SM_table_long",
        "table_long_dec": "Props/SM_table_long_decorated_A",
        "table_med_cloth": "Props/SM_table_medium_tablecloth_decorated_B",
        "table_small": "Props/SM_table_small",
        "table_small_dec": "Props/SM_table_small_decorated_A",
        "shelf_large": "Props/SM_shelf_large",
        "shelf_small": "Props/SM_shelf_small",
        "stool": "Props/SM_stool",
        "chair": "Props/SM_chair",
        "chest": "Props/SM_chest",
        "chest_gold": "Props/SM_chest_gold",
        "coin_lg": "Props/SM_coin_stack_large",
        "coin_md": "Props/SM_coin_stack_medium",
        "coin_sm": "Props/SM_coin_stack_small",
        "coin": "Props/SM_coin",
        "keg": "Props/SM_keg",
        "keg_dec": "Props/SM_keg_decorated",
        "barrel": "Props/SM_barrel_large",
        "barrel_dec": "Props/SM_barrel_large_decorated",
        "barrel_sm": "Props/SM_barrel_small",
        "barrel_stack": "Props/SM_barrel_small_stack",
        "candle": "Props/SM_candle",
        "candle_thin": "Props/SM_candle_thin",
        "candle_triple": "Props/SM_candle_triple",
        "torch": "Props/SM_torch_lit",
        "bottle_a": "Props/SM_bottle_A_labeled_brown",
        "bottle_b": "Props/SM_bottle_B_green",
        "bottle_c": "Props/SM_bottle_C_green",
        "box_sm": "Props/SM_box_small",
        "box_lg": "Props/SM_box_large",
        "box_dec": "Props/SM_box_small_decorated",
        "crates": "Props/SM_crates_stacked",
        "key": "Props/SM_key",
        "keyring": "Props/SM_keyring",
        "sword_gold": "Props/SM_sword_shield_gold",
        "sword": "Props/SM_sword_shield",
        "rubble": "Props/SM_rubble_half",
        "barrier": "Walls/SM_barrier_column",
        "trunk": "Props/SM_trunk_medium_A",
    }
    meshes = {}
    for key, rel in rels.items():
        meshes[key] = load_mesh(rel)
    log("=== mesh bounds ===")
    for key in ("floor", "wall", "doorway", "gated", "stairs", "stairs_narrow", "stairs_wide", "pillar", "table_long"):
        m = meshes.get(key)
        if m:
            sx, sy, sz = bbox_size(m)
            log("bbox %s = %.1f x %.1f x %.1f" % (key, sx, sy, sz))
    return meshes


def _xy(ix, iy, ox=0.0, oy=0.0):
    return (ix * TILE + ox, iy * TILE + oy)


def build_shop(meshes):
    log("=== BUILD MAP_SHOP ===")
    create_empty_level(SHOP_MAP)
    b = LevelBuilder("Shop", meshes)

    # Entrance south, shop hall in the middle, storage dead-end north,
    # exit folded north on the east side (not opposite the entrance).
    b.add_cells([(0, 0)], "dirt")          # entrance landing
    b.add_cells([(0, 1)], "floor")         # vestibule
    b.add_cells([(1, 1), (2, 1), (3, 1), (1, 2), (2, 2), (3, 2)], "floor")
    b.add_cells([(1, 3), (2, 3)], "floor_rocks")  # storage dead-end
    b.add_cells([(4, 2)], "floor")         # exit hall
    b.add_cells([(4, 3)], "dirt")          # exit landing

    # Distinct openings
    b.set_edge(0, 0, "S", "doorway")       # entrance face (south)
    # Internal edges automatically stay open.
    b.set_edge(4, 3, "N", "gated")         # exit face (north)

    # Visual variety on some outer walls
    b.set_edge(1, 2, "W", "wall_shelves")
    b.set_edge(1, 3, "W", "wall_shelves")
    b.set_edge(3, 1, "S", "wall_scaffold")
    b.set_edge(2, 1, "S", "wall_cracked")
    b.set_edge(3, 2, "N", "wall_shelves")

    b.place_floors()
    b.place_walls()

    # Entrance stairs on the south landing, facing south (approach from south, walk north in).
    b.prop("stairs_wide", _xy(0, 0, 0, -40), yaw=180.0, tag="EntranceStairs")
    # Exit stairs on the north landing, facing north (way out).
    b.prop("stairs_narrow", _xy(4, 3, 0, 40), yaw=0.0, tag="ExitStairs")

    # Merchant counter along the south side of the hall, leaving a walk aisle north of it.
    b.prop("table_long_dec", _xy(2, 1, 0, -40), yaw=90.0, tag="Counter")
    b.prop("table_long", _xy(1, 1, 80, -40), yaw=90.0, tag="Counter")
    b.prop("stool", _xy(2, 1, 0, 70), yaw=180.0, tag="Stool")
    b.prop("chair", _xy(1, 1, 80, 80), yaw=180.0, tag="Chair")

    # Wares on / around the counter
    b.prop("coin_lg", _xy(2, 1, -40, -40), z=85.0, tag="Coins")
    b.prop("coin_md", _xy(2, 1, 30, -20), z=85.0, tag="Coins")
    b.prop("coin_sm", _xy(1, 1, 40, -40), z=85.0, tag="Coins")
    b.prop("coin", _xy(1, 1, 110, -10), z=85.0, tag="Coins")
    b.prop("candle_triple", _xy(2, 1, 70, -40), z=85.0, tag="Candle")
    b.prop("candle", _xy(1, 1, 10, -20), z=85.0, tag="Candle")
    b.prop("keyring", _xy(2, 1, -90, -10), z=85.0, tag="Keys")
    b.prop("key", _xy(2, 1, -70, -30), z=85.0, tag="Keys")
    b.prop("bottle_a", _xy(1, 1, 50, 0), z=85.0, tag="Wares")
    b.prop("bottle_b", _xy(2, 1, 100, -20), z=85.0, tag="Wares")
    b.prop("box_dec", _xy(3, 1, -80, -40), z=0.0, tag="Wares")

    # Shelves and stock along the south-east and west walls of the hall
    b.prop("shelf_large", _xy(3, 1, 140, -80), yaw=90.0, tag="Shelf")
    b.prop("shelf_small", _xy(3, 2, 140, 0), yaw=90.0, tag="Shelf")
    b.prop("chest", _xy(3, 1, 120, 80), yaw=-90.0, tag="Chest")
    b.prop("chest_gold", _xy(3, 2, 120, 120), yaw=-90.0, tag="Chest")
    b.prop("crates", _xy(1, 1, -120, -100), yaw=0.0, tag="Crates")
    b.prop("box_lg", _xy(1, 2, -130, -80), yaw=15.0, tag="Boxes")
    b.prop("box_sm", _xy(1, 2, -120, 40), yaw=-20.0, tag="Boxes")

    # Kegs / barrels in the west-north corner of the hall, not blocking the aisle
    b.prop("keg_dec", _xy(1, 2, -100, 140), yaw=20.0, tag="Keg")
    b.prop("keg", _xy(1, 2, -30, 150), yaw=0.0, tag="Keg")
    b.prop("barrel_dec", _xy(2, 2, -40, 150), yaw=-15.0, tag="Barrel")
    b.prop("barrel_stack", _xy(3, 2, 40, 150), yaw=10.0, tag="Barrel")

    # Storage dead-end (browse only)
    b.prop("shelf_large", _xy(1, 3, -140, 40), yaw=-90.0, tag="StoreShelf")
    b.prop("shelf_small", _xy(1, 3, -140, -80), yaw=-90.0, tag="StoreShelf")
    b.prop("trunk", _xy(1, 3, 40, 140), yaw=180.0, tag="Trunk")
    b.prop("crates", _xy(2, 3, 120, 120), yaw=-30.0, tag="StoreCrates")
    b.prop("barrel", _xy(2, 3, 140, -40), yaw=0.0, tag="StoreBarrel")
    b.prop("barrel_sm", _xy(2, 3, 80, -80), yaw=25.0, tag="StoreBarrel")
    b.prop("box_dec", _xy(2, 3, 0, 140), yaw=0.0, tag="StoreBox")
    b.prop("candle_thin", _xy(1, 3, 20, 60), z=40.0, tag="Candle")
    b.prop("coin_sm", _xy(2, 3, -60, 40), z=0.0, tag="Coins")

    # Vestibule dress
    b.prop("barrel_sm", _xy(0, 1, -130, 80), yaw=10.0, tag="Barrel")
    b.prop("candle_triple", _xy(0, 1, 120, 120), z=0.0, tag="Candle")

    # Torches / warm lights (hall, storage, entrance, exit)
    b.torch(_xy(0, 0, -150, 0), yaw=90.0, intensity=4000, radius=1100)
    b.torch(_xy(0, 1, 0, 160), yaw=180.0, intensity=3800, radius=1200)
    b.torch(_xy(2, 1, 0, -170), yaw=0.0, intensity=5200, radius=1500)
    b.torch(_xy(1, 2, -170, 0), yaw=90.0, intensity=4200, radius=1300)
    b.torch(_xy(3, 2, 170, 0), yaw=-90.0, intensity=4200, radius=1300)
    b.torch(_xy(2, 3, 0, 170), yaw=180.0, intensity=3600, radius=1100)
    b.torch(_xy(4, 2, 0, -160), yaw=0.0, intensity=3800, radius=1100)
    b.torch(_xy(4, 3, 0, 160), yaw=180.0, intensity=4200, radius=1100)

    add_common_lights(b, warm=True)
    add_player_start("Shop_PlayerStart", _xy(0, 0, 0, 20), yaw=90.0)

    actors = list(unreal.EditorLevelLibrary.get_all_level_actors())
    log("Map_Shop actor count=%d spawned_labelled~=%d" % (len(actors), b.n))
    save_level(SHOP_MAP)
    return True


def build_boss(meshes):
    log("=== BUILD MAP_BOSS ===")
    create_empty_level(BOSS_MAP)
    b = LevelBuilder("Boss", meshes)

    # Short west approach, then a 4x3 arena. Exit is on the east, then turns
    # north — not opposite the entrance. Treasure alcove is a north dead-end.
    b.add_cells([(0, 1)], "dirt")                 # entrance landing
    b.add_cells([(1, 1)], "dirt")                 # corridor
    b.add_cells([(2, 1)], "floor_rocks")          # antechamber
    arena = []
    for ix in range(2, 6):
        for iy in range(2, 5):
            arena.append((ix, iy))
    b.add_cells(arena, "floor")
    b.add_cells([(3, 5), (4, 5)], "dirt_rocky")   # treasure alcove
    b.add_cells([(6, 3)], "floor_rocks")          # exit hall
    b.add_cells([(6, 4)], "dirt")                 # exit landing

    b.set_edge(0, 1, "W", "doorway")
    b.set_edge(6, 4, "N", "gated")
    b.set_edge(2, 1, "S", "wall_cracked")
    b.set_edge(2, 2, "W", "wall_scaffold")
    b.set_edge(2, 4, "W", "arched")
    b.set_edge(5, 2, "S", "wall_cracked")
    b.set_edge(5, 4, "E", "wall_scaffold")

    b.place_floors()
    b.place_walls()

    # Entrance stairs facing west (approach from west, walk east in).
    b.prop("stairs", _xy(0, 1, -40, 0), yaw=-90.0, tag="EntranceStairs")
    # Exit stairs facing north (way out).
    b.prop("stairs_narrow", _xy(6, 4, 0, 40), yaw=0.0, tag="ExitStairs")

    # Pillars around the arena, leaving the center open for a later boss spawn.
    # Arena covers x=2..5, y=2..4. Center ~ (3.5, 3) * 400 = (1400, 1200).
    pillar_spots = [
        (2, 2, 110, 110, 0.0),
        (5, 2, -110, 110, 0.0),
        (2, 4, 110, -110, 0.0),
        (5, 4, -110, -110, 0.0),
        (2, 3, 110, 0, 0.0),
        (5, 3, -110, 0, 0.0),
    ]
    for ix, iy, ox, oy, yaw in pillar_spots:
        b.prop("pillar", _xy(ix, iy, ox, oy), yaw=yaw, tag="Pillar")

    # Dais / altar focus on the north side of the arena, walkway around to the alcove.
    b.prop("table_long_dec", _xy(3, 4, 200, 40), yaw=90.0, tag="Dais")
    b.prop("table_small_dec", _xy(4, 4, -80, 40), yaw=0.0, tag="Altar")
    b.prop("candle_triple", _xy(3, 4, 200, 40), z=85.0, tag="AltarCandle")
    b.prop("candle", _xy(3, 4, 120, 10), z=85.0, tag="AltarCandle")
    b.prop("candle_thin", _xy(4, 4, -40, 10), z=85.0, tag="AltarCandle")
    b.prop("sword_gold", _xy(4, 4, 40, 40), yaw=0.0, tag="AltarRelic")
    b.prop("barrier", _xy(3, 4, 20, -40), yaw=90.0, tag="DaisRail")
    b.prop("barrier", _xy(4, 4, 80, -40), yaw=90.0, tag="DaisRail")

    # Treasure alcove dead-end (does not skip the arena)
    b.prop("chest_gold", _xy(3, 5, 0, 80), yaw=180.0, tag="Treasure")
    b.prop("chest", _xy(4, 5, 80, 80), yaw=180.0, tag="Treasure")
    b.prop("coin_lg", _xy(4, 5, -40, 40), z=0.0, tag="Treasure")
    b.prop("coin_md", _xy(3, 5, 80, 40), z=0.0, tag="Treasure")
    b.prop("candle_triple", _xy(3, 5, -80, 80), z=0.0, tag="Candle")
    b.prop("rubble", _xy(4, 5, 140, -40), yaw=-30.0, tag="Rubble")

    # Approach dress — keep the corridor walkable
    b.prop("barrel", _xy(1, 1, 0, 140), yaw=15.0, tag="Barrel")
    b.prop("rubble", _xy(1, 1, 80, -140), yaw=40.0, tag="Rubble")
    b.prop("torch", _xy(0, 1, 0, 150), yaw=180.0, z=150.0, tag="Torch")
    b.prop("sword", _xy(2, 1, -120, -130), yaw=20.0, tag="Prop")
    b.prop("barrel_sm", _xy(2, 1, 140, -120), yaw=-10.0, tag="Barrel")

    # Light dress around the arena, not in the open center
    b.torch(_xy(0, 1, -150, 0), yaw=90.0, intensity=4000, radius=1100, color=(1.0, 0.5, 0.22), temp=2500)
    b.torch(_xy(1, 1, 0, -160), yaw=0.0, intensity=3600, radius=1100, color=(1.0, 0.5, 0.22), temp=2500)
    b.torch(_xy(2, 1, 0, -160), yaw=0.0, intensity=4000, radius=1200, color=(1.0, 0.5, 0.22), temp=2500)
    b.torch(_xy(2, 2, -160, -160), yaw=45.0, intensity=4800, radius=1400, color=(1.0, 0.45, 0.18), temp=2400)
    b.torch(_xy(5, 2, 160, -160), yaw=-45.0, intensity=4800, radius=1400, color=(1.0, 0.45, 0.18), temp=2400)
    b.torch(_xy(2, 4, -160, 160), yaw=135.0, intensity=5000, radius=1400, color=(1.0, 0.45, 0.18), temp=2400)
    b.torch(_xy(5, 4, 160, 160), yaw=-135.0, intensity=5000, radius=1400, color=(1.0, 0.45, 0.18), temp=2400)
    # Stronger warm light over the dais
    b.torch(_xy(3, 4, 200, 80), yaw=180.0, intensity=6500, radius=1600, color=(1.0, 0.48, 0.2), temp=2300, z=180.0)
    b.torch(_xy(6, 3, 160, 0), yaw=-90.0, intensity=3800, radius=1100, color=(1.0, 0.5, 0.22), temp=2500)
    b.torch(_xy(6, 4, 0, 160), yaw=180.0, intensity=4200, radius=1100, color=(1.0, 0.5, 0.22), temp=2500)
    b.torch(_xy(3, 5, -150, 0), yaw=90.0, intensity=3200, radius=1000, color=(1.0, 0.55, 0.25), temp=2600)

    add_common_lights(b, warm=True)
    add_player_start("Boss_PlayerStart", _xy(0, 1, 20, 0), yaw=90.0)

    actors = list(unreal.EditorLevelLibrary.get_all_level_actors())
    log("Map_Boss actor count=%d spawned_labelled~=%d" % (len(actors), b.n))
    save_level(BOSS_MAP)
    return True


def quit_editor():
    log("requesting editor quit")
    flush()
    try:
        unreal.SystemLibrary.quit_editor()
        return
    except Exception as e:
        log("quit_editor(): " + str(e))
    try:
        world = unreal.EditorLevelLibrary.get_editor_world()
        unreal.SystemLibrary.execute_console_command(world, "QUIT_EDITOR")
        return
    except Exception as e:
        log("QUIT_EDITOR: " + str(e))
    flush()


def main():
    log("=== build_map_shop_boss start ===")
    try:
        log("engine world at start: " + current_world_path())
        meshes = load_all_meshes()
        missing = [k for k, v in meshes.items() if v is None]
        if missing:
            log("WARNING missing meshes: " + ",".join(missing))
        required = ["floor", "wall", "doorway", "gated", "stairs", "stairs_narrow"]
        if any(meshes.get(k) is None for k in required):
            raise RuntimeError("Required modular meshes missing, aborting")
        build_shop(meshes)
        build_boss(meshes)
        log("SHOP_EXISTS=" + str(unreal.EditorAssetLibrary.does_asset_exist(SHOP_MAP)))
        log("BOSS_EXISTS=" + str(unreal.EditorAssetLibrary.does_asset_exist(BOSS_MAP)))
        log("DONE")
    except Exception as e:
        log("FATAL " + str(e))
        log(traceback.format_exc())
    flush()
    # Never quit an already-open interactive editor.
    # Only request exit when launched via -unattended / commandlet.
    try:
        import os
        force_quit = os.environ.get("ASHENRIFT_QUIT_EDITOR") == "1"
    except Exception:
        force_quit = False
    if force_quit:
        quit_editor()
    else:
        log("leaving interactive editor open")
        flush()


if __name__ == "__main__":
    main()
