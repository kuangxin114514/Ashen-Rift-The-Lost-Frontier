"""Create plain Actor-parent room BPs with SCS meshes (Viewport + Components work)."""
import unreal

ROOMS = '/Game/AshenRift/Rooms'
MAP = '/Game/AshenRift/Maps/L_RandomDungeon'
lib = unreal.EditorAssetLibrary
level = unreal.EditorLevelLibrary

SPECS = [
    ('BP_Room_Pillars', 0, 'Pillars', 4),
    ('BP_Room_Barricade', 1, 'Barricade', 3),
    ('BP_Room_Cross', 2, 'Cross', 4),
]

if not lib.does_directory_exist(ROOMS):
    lib.make_directory(ROOMS)

# Need a level to spawn-verify
level.load_level(MAP)

classes = []
for name, kind, label, min_meshes in SPECS:
    path = ROOMS + '/' + name
    ok = unreal.ARDungeonEditorLibrary.create_editable_room_blueprint(ROOMS, name, kind)
    unreal.log('CreateEditableRoomBlueprint(%s kind=%d)=%s' % (name, kind, ok))
    if not ok:
        raise RuntimeError('CreateEditableRoomBlueprint failed for ' + name)

    asset = lib.load_asset(path)
    if asset is None:
        raise RuntimeError('Missing asset ' + path)
    lib.save_loaded_asset(asset)

    gc = lib.load_blueprint_class(path)
    if gc is None:
        raise RuntimeError('No generated class for ' + path)

    is_room_cpp = False
    try:
        is_room_cpp = gc.is_child_of(unreal.ARDungeonRoom)
    except Exception:
        pass
    unreal.log('HIERARCHY %s is_child_of_ARDungeonRoom=%s class=%s' % (name, is_room_cpp, gc.get_name()))
    if is_room_cpp:
        raise RuntimeError('%s still inherits ARDungeonRoom — want plain Actor' % name)

    # Spawn to verify SCS actually builds meshes (CDO query is unreliable for fresh Actor BPs)
    actor = level.spawn_actor_from_class(gc, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0))
    if actor is None:
        raise RuntimeError('Spawn failed for ' + name)
    meshes = actor.get_components_by_class(unreal.StaticMeshComponent)
    unreal.log('SPAWN_VERIFY %s meshComps=%d root=%s label=%s' % (
        name, len(meshes), actor.root_component, actor.get_actor_label()))
    for c in meshes:
        sm = c.static_mesh
        unreal.log('  - %s mesh=%s loc=%s' % (c.get_name(), sm.get_name() if sm else None, c.relative_location))
    if len(meshes) < min_meshes or actor.root_component is None:
        level.destroy_actor(actor)
        raise RuntimeError('Spawned actor missing meshes for ' + name)
    level.destroy_actor(actor)
    classes.append(gc)

updated = False
for actor in level.get_all_level_actors():
    cname = actor.get_class().get_name()
    if cname.startswith('ARDungeonDirector') or actor.get_actor_label() == 'RandomRoomDirector':
        actor.set_editor_property('room_layouts', classes)
        actor.set_actor_label('RandomRoomDirector')
        updated = True
        unreal.log('Updated RandomRoomDirector.RoomLayouts -> Actor BPs (%d)' % len(classes))
if updated:
    level.save_current_level()
else:
    unreal.log_warning('RandomRoomDirector not found')

lib.save_directory(ROOMS, only_if_is_dirty=False, recursive=True)
unreal.log('ASHEN_RIFT_ACTOR_ROOM_BPS_READY')