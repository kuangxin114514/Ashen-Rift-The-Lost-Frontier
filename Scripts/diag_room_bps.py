import unreal

def inspect(path):
    unreal.log('==== INSPECT ' + path + ' ====')
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.log_error('MISSING ' + path)
        return
    bp = unreal.EditorAssetLibrary.load_asset(path)
    unreal.log('asset_class=' + bp.get_class().get_name())
    try:
        unreal.log('parent=' + str(bp.get_editor_property('parent_class')))
    except Exception as e:
        unreal.log_warning('parent err ' + str(e))
    try:
        unreal.log('generated=' + str(bp.get_editor_property('generated_class')))
    except Exception as e:
        unreal.log_warning('generated err ' + str(e))
    try:
        unreal.log('bp_type=' + str(bp.get_editor_property('blueprint_type')))
    except Exception as e:
        unreal.log_warning('bp_type err ' + str(e))
    try:
        scs = bp.get_editor_property('simple_construction_script')
        unreal.log('scs=' + str(scs))
        if scs:
            try:
                nodes = scs.get_editor_property('all_nodes')
                unreal.log('scs_all_nodes=' + str(len(nodes) if nodes else 0))
            except Exception as e:
                unreal.log_warning('scs nodes err ' + str(e))
            try:
                root = scs.get_editor_property('default_scene_root_node')
                unreal.log('scs_root=' + str(root))
            except Exception as e:
                unreal.log_warning('scs root err ' + str(e))
    except Exception as e:
        unreal.log_warning('scs err ' + str(e))
    try:
        gc = unreal.EditorAssetLibrary.load_blueprint_class(path)
        unreal.log('load_blueprint_class=' + str(gc))
        if gc:
            cdo = unreal.get_default_object(gc)
            unreal.log('cdo=' + str(cdo))
            comps = cdo.get_components_by_class(unreal.ActorComponent)
            unreal.log('cdo_comp_count=' + str(len(comps)))
            for c in comps:
                unreal.log('  comp ' + c.get_name() + ' class=' + c.get_class().get_name())
            root = cdo.root_component
            unreal.log('cdo_root=' + str(root))
    except Exception as e:
        unreal.log_warning('cdo err ' + str(e))
    # native parent CDO
    for cls_name in ['ARDungeonRoom_Pillars', 'ARDungeonRoom_Barricade', 'ARDungeonRoom_Cross', 'ARDungeonRoom']:
        cls = getattr(unreal, cls_name, None)
        if cls is None:
            continue
        try:
            native = unreal.get_default_object(cls)
            comps = native.get_components_by_class(unreal.ActorComponent)
            unreal.log('native ' + cls_name + ' comps=' + str(len(comps)))
            for c in comps:
                unreal.log('  N ' + c.get_name())
        except Exception as e:
            unreal.log_warning('native ' + cls_name + ' err ' + str(e))

for n in ['BP_Room_Pillars', 'BP_Room_Barricade', 'BP_Room_Cross']:
    inspect('/Game/AshenRift/Rooms/' + n)
unreal.log('ASHEN_RIFT_BP_DIAG_DONE')
