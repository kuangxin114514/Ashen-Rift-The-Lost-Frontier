import unreal
level = unreal.EditorLevelLibrary
assert level.load_level('/Game/AshenRift/Maps/L_RandomDungeon')
counts = {}
samples = []
for actor in level.get_all_level_actors():
    try:
        label = actor.get_actor_label()
    except Exception:
        continue
    key = label.split('_')[0] if '_' in label else label
    # finer buckets
    if label.startswith('KayKitShell_'):
        bucket = 'KayKitShell'
    elif label.startswith('DoorLeaf_'):
        bucket = 'DoorLeaf'
    elif label.startswith('SideWall'):
        bucket = 'SideWall_GREY'
    elif label.startswith('DoorWall'):
        bucket = 'DoorWall_GREY'
    elif label.startswith('Floor_'):
        bucket = 'Floor_GREY'
    elif label in ('Camp_Back', 'Dungeon_End'):
        bucket = 'End_GREY'
    elif label.startswith('BP_Room') or 'Room' in label:
        bucket = 'Roomish:' + label
    elif label == 'RandomRoomDirector':
        bucket = 'Director'
    else:
        continue
    counts[bucket] = counts.get(bucket, 0) + 1
    if len(samples) < 15 and bucket.startswith(('KayKit', 'Door', 'Side', 'Floor', 'End')):
        samples.append(label)
unreal.log('SHELL_COUNTS ' + str(counts))
unreal.log('SHELL_SAMPLES ' + str(samples))
# Director room layouts
for actor in level.get_all_level_actors():
    if actor.get_actor_label() == 'RandomRoomDirector':
        layouts = actor.get_editor_property('room_layouts')
        unreal.log('DIRECTOR_LAYOUTS n=%d %s' % (len(layouts), [str(c) for c in layouts]))
unreal.log('ASHEN_RIFT_SHELL_AUDIT_DONE')
