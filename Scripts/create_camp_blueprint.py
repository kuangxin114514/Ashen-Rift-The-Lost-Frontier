"""Create editable BP_Camp (Actor parent) and assign it on RandomRoomDirector.CampLayout."""
import unreal

ROOMS = '/Game/AshenRift/Rooms'
MAP = '/Game/AshenRift/Maps/L_RandomDungeon'
lib = unreal.EditorAssetLibrary
level = unreal.EditorLevelLibrary

if not lib.does_directory_exist(ROOMS):
    lib.make_directory(ROOMS)

level.load_level(MAP)

name = 'BP_Camp'
path = ROOMS + '/' + name
ok = unreal.ARDungeonEditorLibrary.create_editable_camp_blueprint(ROOMS, name)
unreal.log('CreateEditableCampBlueprint=%s' % ok)
if not ok:
    raise RuntimeError('CreateEditableCampBlueprint failed')

asset = lib.load_asset(path)
if asset is None:
    raise RuntimeError('Missing ' + path)
lib.save_loaded_asset(asset)

gc = lib.load_blueprint_class(path)
if gc is None:
    raise RuntimeError('No generated class for ' + path)

# Verify spawn has meshes
actor = level.spawn_actor_from_class(gc, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0))
if actor is None:
    raise RuntimeError('Spawn failed for BP_Camp')
meshes = actor.get_components_by_class(unreal.StaticMeshComponent)
unreal.log('SPAWN_VERIFY BP_Camp meshComps=%d' % len(meshes))
for c in meshes:
    sm = c.static_mesh
    unreal.log('  - %s mesh=%s loc=%s' % (c.get_name(), sm.get_name() if sm else None, c.relative_location))
if len(meshes) < 3:
    level.destroy_actor(actor)
    raise RuntimeError('BP_Camp missing starter meshes')
level.destroy_actor(actor)

updated = False
for actor in level.get_all_level_actors():
    cname = actor.get_class().get_name()
    if cname.startswith('ARDungeonDirector') or actor.get_actor_label() == 'RandomRoomDirector':
        actor.set_editor_property('camp_layout', gc)
        actor.set_editor_property('camp_origin', unreal.Vector(0.0, 0.0, 0.0))
        actor.set_actor_label('RandomRoomDirector')
        updated = True
        unreal.log('Updated RandomRoomDirector.CampLayout -> BP_Camp')
if updated:
    level.save_current_level()
else:
    unreal.log_warning('RandomRoomDirector not found')

lib.save_directory(ROOMS, only_if_is_dirty=False, recursive=True)
unreal.log('ASHEN_RIFT_CAMP_BP_READY')
