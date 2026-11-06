import unreal
lib = unreal.EditorAssetLibrary
level = unreal.EditorLevelLibrary
path = '/Game/AshenRift/Rooms/BP_Camp'
assert lib.does_asset_exist(path), 'BP_Camp missing'
gc = lib.load_blueprint_class(path)
assert gc is not None
level.load_level('/Game/AshenRift/Maps/L_RandomDungeon')
actor = level.spawn_actor_from_class(gc, unreal.Vector(0,0,0))
meshes = actor.get_components_by_class(unreal.StaticMeshComponent)
unreal.log('DIAG_CAMP meshes=%d parent=%s' % (len(meshes), gc.get_name()))
for c in meshes:
    sm = c.static_mesh
    unreal.log('  %s -> %s' % (c.get_name(), sm.get_name() if sm else None))
level.destroy_actor(actor)
found = False
for a in level.get_all_level_actors():
    if a.get_actor_label() == 'RandomRoomDirector' or a.get_class().get_name().startswith('ARDungeonDirector'):
        cl = a.get_editor_property('camp_layout')
        origin = a.get_editor_property('camp_origin')
        unreal.log('DIRECTOR camp_layout=%s origin=%s' % (cl, origin))
        found = True
if not found:
    unreal.log_warning('DIRECTOR missing')
unreal.log('ASHEN_RIFT_CAMP_DIAG_OK')
