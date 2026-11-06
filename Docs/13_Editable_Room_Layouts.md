# Editable Room Layouts (Actor Blueprints)

Room prefabs live at `Content/AshenRift/Rooms/`:

- `BP_Room_Pillars`
- `BP_Room_Barricade`
- `BP_Room_Cross`

**Parent class is `Actor`**, not `ARDungeonRoom_*`. Open any of them and you should see Viewport + Components. Drag the Static Mesh components (W/E/R) to edit layouts, then Compile & Save.

`RandomRoomDirector` on `L_RandomDungeon` has **Room Layouts** pointing at these three BPs. Duplicate a BP, edit it, add it to Room Layouts to expand the random pool.

Regenerate (editor Python):

`Scripts/create_room_blueprints.py`

Uses `ARDungeonEditorLibrary.CreateEditableRoomBlueprint` which builds an Actor BP with SCS meshes.

Camp hub props: see `14_Camp_Room_Blueprint.md` (BP_Camp, Director **Camp Layout**).
