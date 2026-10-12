# Native widescreen DX — development checkpoint

This is not a completed widescreen port. Release builds keep the existing display.
The Debug-only setting is a scenery prototype; adjacent enemies are NOT rendered
or simulated. Do not present it as a playable, faithful widescreen mode.

## Implemented

- Pure C overworld room parsing, macro expansion, persistent room changes, CGB
  attribute selection, room-specific patterns and palettes from the supplied ROM.
- Native entity spawn definitions with global 16-bit coordinates, load order,
  cleared-entity masks and the original 16-slot loading limit. These are spawn
  definitions, not running entities; they are not drawn as frozen placeholders.
- Read-only neighboring-scenery composition, coherent frame publication and Metal
  texture support for up to 512×144. The original 160×144 center is copied intact.
- Fallback for transitions, interiors, unsupported revisions, palette changes,
  incompatible animation groups and island boundaries.

Only the French Rev 1 SHA-256
`211a1d1559dc2684d63d87f62a6b1e9109201a6383b4004e9dd5e18ee926ee83`
is enabled by the caller. DXWorld functions require that verified profile.

## Verification

Run `python3 Scripts/test_core.py`, then `python3 Scripts/test_world.py`
and `python3 Scripts/test_entities.py`.
The latter executes original ROM routines in isolated contexts as an oracle:

- 256 rooms × 8 persistent-state combinations: blocks and attributes.
- 256 stable-palette room renders: all 20,480 pixels against the original tilemap
  conversion and PPU, using varied tile patterns to exercise bank selection/flips.
- 256 rooms × 8 cleared masks: entity count, position, type and spawn order.
- Widths 161, 319, 320 and 512: center preservation and output bounds for eligible
  rooms; transitions reject the extension.

These checks do not prove animation, AI or transitions correct in widescreen.
No Xcode/iPhone validation has been performed in this Linux workspace.

## Remaining engineering

The game still runs its statically translated GB logic and hardware runtime.
A faithful expanded field requires a persistent entity model for all visible
rooms, the original activation/respawn rules, native AI and collision or a proven
bridge to the translated routines, world-space sprite rendering (including
priority/occlusion), and coherent camera/room transitions. Merely increasing the
16-slot pool changes gameplay and is insufficient. Shared animated VRAM also
needs replacement with per-scene animation state. Interiors and special scenes
need their own topology rather than overworld adjacency.

Before enabling normal play, test combat and projectile collisions on both sides
of the old boundary, entity lifetime and save/reload, scrolling in all directions,
room changes, indoor entrances, palettes, cutscenes, and actual iPhone layouts.

## Entity primitives — second checkpoint

`DXEntity` adds native signed world-coordinate motion and single-sprite drawing.
All 65,536 speed/fraction combinations match the French bank 3 routine at 7F3C.
Position extends to signed 32-bit coordinates; arithmetic overflow fails without
changing state. The original low-byte movement result remains identical.

The sprite primitive accepts resolved world positions, tile IDs and flags. It
supports CGB palettes/banks, transparency, flips, 8x8/8x16 sprites and background
priority. `dx_world_draw_layers` now retains raw background color/priority data,
verified against the original room tilemap and the project's PPU. RGB alone was
insufficient to decide whether scenery should cover a sprite.

10,240 comparisons against the project's existing CGB PPU cover all flag values,
both sprite heights, background priority enabled/disabled, clipping and camera
translation into a 512-pixel field. This is a software differential oracle,
not physical Game Boy or iPhone validation. Invalid dimensions/capacities and
extreme signed camera positions are also checked.

These primitives are not connected to neighboring AI. They deliberately do not
invent enemy behavior, duplicate active entities, or display spawn records as
static actors. Multiple-entity ordering, sprite-sheet residency, special drawing
paths and lifecycle still need integration and validation.

Confirmed constraints in original logic:

- Movement accumulates fractional speed before updating the 8-bit position;
  position-sign tables have separate handling during room transitions.
- Pair/rectangle draw helpers ultimately write 8-bit OAM coordinates; transition
  filtering can discard an entity before the sprite reaches the PPU.
- Arrows consult room-local events (including the dungeon eye-statue trigger),
  collision results and a shared projectile count; extending only their X range
  would not preserve these interactions.
- Adjacent-room simulation must bind room-local state and collision maps to each
  entity, while maintaining player interaction and the original respawn rules.
