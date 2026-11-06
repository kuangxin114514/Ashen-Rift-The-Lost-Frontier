# Editable Camp Layout (BP_Camp)

Camp props live at `Content/AshenRift/Rooms/BP_Camp`.

**Parent class is `Actor`** (same as combat room BPs). Open it for Viewport + Components, drag KayKit meshes (W/E/R), Compile & Save.

## When it spawns

`RandomRoomDirector` (`AARDungeonDirector`) on `L_RandomDungeon`:

- **Camp Layout** — soft class for `BP_Camp` (assignable in Details)
- **Camp Origin** — world transform of the camp cell (default `(0,0,0)`, slot 0 / same cell as `Camp_Back` + `CampSpawn`)

Spawn hooks:

1. **Game start** — `AARDungeonDirector::BeginPlay` → `EnsureCampLayout()`
2. **Evacuate / return to hub** — after `ReturnToHub` + `ClearRunActors`, `EnsureCampLayout()` again, then player teleports to `CampSpawn` `(-700,0,100)`

Combat rooms (`Room Layouts`) are unchanged. Camp is **not** in the random combat pool.

## Edit / assign

1. Content Browser → `AshenRift/Rooms/BP_Camp`
2. Add Static Mesh components from `AshenRift/KayKit/Props` (Furniture / Containers / Lights / Decor)
3. Compile & Save
4. Select **RandomRoomDirector** in `L_RandomDungeon` → set **Camp Layout** to `BP_Camp` if empty → Save level

## Regenerate starter BP

Editor: **File → Execute Python Script** → `Scripts/create_camp_blueprint.py`

Uses `ARDungeonEditorLibrary.CreateEditableCampBlueprint` (table, chair, chest, barrel, crate, torch, banner).
