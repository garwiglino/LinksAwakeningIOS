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

## Collision and roaming decision checkpoint

`DXCollision` ports the French Rev 1 `ApplyEntityCollisionWithObject` routine
(bank 3, 7AD1), including fish/water-tektite restrictions, fine collision shapes,
open doors, ledges, switch blocks, hookshot attachment, the French hookshot
immunity fix, per-direction flags and entity-specific bypasses. It deliberately
separates contact classification from ground effects and damage.

Validation: 589,824 calls to the original routine, spanning all object IDs,
three physics-table groups, twelve entity types and 64 state/geometry profiles.
No mismatches in blocking, collision flags, ledge state, hookshot state/unloading,
Link immunity and sampled coordinates. This is structured coverage, not an
exhaustive proof of all possible gameplay state combinations.

Movement and wall resolution are connected in `dx_collision_move_walls`, retaining
fractional movement and the original X-before-Y response. 4,096 integrated cases
match original movement plus the wall phase. The collision map lookup uses world
coordinates: 40,960 samples across adjacent rooms, an actual wall across the old
screen edge, passage into an empty neighbor, and atomic rollback when neighboring
geometry is unavailable are checked. No horizontal island wrapping is permitted.
Ground effects (drowning, falling, conveyors, grass and splashes) remain separate
and are not implemented by this wall-motion function.

`DXRoaming` ports the common post-combat movement/firing decision for Octoroks,
Moblins and Iron Masks. 24,576 original decision traces agree on state, cooldown,
direction, velocities, collision flags, RNG consumption and delegated action.
The tests cover movement, ground checks, arrow requests, rock requests and waits.
Target direction uses world distances without the original 8-bit aliasing.
The RNG remains an explicit input owned by the scheduler. The active state
copy (hActiveEntityState) is kept distinct from the stored entity state; tests
also cover disagreement between them. An original quirk is
preserved: the collision-triggered pause writes through the collision-table
pointer, while the timer-triggered pause writes through the timer pointer.

These modules are not a complete live enemy handler. Timers, damage, recoil,
projectile allocation/execution, ground effects, animation, room activation and
sprite-sheet residency still require integration. Neither collisions nor this
three-family decision phase mean all neighboring entities work in gameplay.
The public widescreen setting remains disabled outside Debug.

Run `Scripts/test_collisions.py` and `Scripts/test_roaming.py` after the core
build; CI runs both. The preceding published checkpoint (0d21a003) passed Debug
simulator compilation, Release device compilation and simulator launch on GitHub.
This does not constitute a physical iPhone gameplay test.


## Ground and integrated physics checkpoint

`DXGround` ports the French Rev 1 ground phase (bank 3, 7897–7A1C):
water/grass status, drowning exceptions, splash requests, conveyor movement,
pit/well falls, fall targets and timers, including side-scrolling velocity
adjustment. Effects are returned as requests; allocating transient effects and
playing their jingles remain the caller's responsibility.

786,432 comparisons cover all object IDs, three physics groups, sixteen entity
types and 64 profiles. The isolated oracle replaces only the next phase's entry
with RET so its fallthrough can be observed; ground instructions are unchanged.

`DXPhysics` integrates optional fractional movement, ground sampling at the
moved position, ground reactions, then X-before-Y wall resolution. Wall rollback
uses the position before the entire step, including conveyor displacement.
All outputs, including deferred audio/visual requests, commit together only when
the required room geometry is available. This entry point accepts overworld
topology only; it rejects indoor and side-scrolling contexts.

524,288 comparisons execute the complete original background routine without
patching its ROM. They cover uniform and spatially mixed terrain, optional
movement, fractional positions and four hitboxes. No mismatches were found in
positions, fractions, terrain state, fall targets/timers, death, collision flags,
wall contact IDs, velocities or effect requests. Additional tests cover water
and pit reactions across the old screen boundary, and total rollback when a
later wall lookup fails after a ground effect was prepared. Existing collision
comparisons also pass after extracting the reusable wall-resolution phase.

Run `Scripts/test_ground.py` and `Scripts/test_physics.py` after the core build;
both are included in CI. These are software comparisons, not an iPhone gameplay
validation. The previous published checkpoint (7eeba98) passed Debug simulator
and Release device builds plus simulator launch.

**Remaining integration:** the app still renders only neighboring scenery.
These physics routines are not yet connected to a live neighboring-entity
scheduler. Shared player damage, enemy lifecycle, projectile execution, entity
sprite residency, transition ownership and room-local events still need to be
implemented and tested before enabling a playable widescreen mode. No claim of
fully functional widescreen follows from this checkpoint.


## Native actor execution and enemy projectile checkpoint

`DXActors` introduces a native actor record and a sixteen-slot roster. Generic
configuration preserves the original selective reset: persistent coordinates and
other untouched fields are retained, while flags, health and hitboxes come from
the verified ROM. It does not substitute for type-specific room initialization.
Timers retain pause rules for dialogs, inventory, transitions, ocarina, gameplay
mode and the dream-bed exception; flashing uses the pre-decrement timer value.

The three-family roaming decision now has an executable post-combat continuation:
movement, ground/wall reactions, animation and real arrow/rock allocation in the
roster. Allocation searches descending slots, retains the original full-roster
behavior, and uses world coordinates for projectile offsets. The entire turn,
including RNG state and projectile allocation, rolls back if physics cannot
resolve a required room. The caller still owns the pre-combat active-state copy
and supplies frame/scanline entropy explicitly.

`DXProjectiles` executes overworld Moblin arrows and Octorok rocks. Contact with
Link precedes the interactivity gate, preserving the original order. Shield
facing, damage buffers, invulnerability, power-up/tunic modifiers, recoil, boots
cancellation and music restoration are modeled as shared player state. Projectile
motion preserves the distinct projectile terrain rules, wall/shield bounce,
spin, gravity and expiry; it must not use enemy walking collision rules. Audio
and Moblin-alert requests are deferred outputs. Missing geometry rolls back the
actor, player damage and effects together. Long-range contact tests no longer
alias every 256 pixels.

Original-ROM differential checks pass:

- 4,016 generic actor configurations (all 251 defined IDs, sixteen initial states).
- 32,768 timer/gameplay states.
- 2,048 arrow/rock allocations, including occupied rosters and all directions.
- 24,576 complete post-combat roaming continuations, including actual spawns.
- 131,072 projectile/player contacts.
- 65,536 projectile movement/terrain/bounce steps.
- 8,192 combined contact, player damage, shield and terrain steps.

These tests execute original routines in isolated contexts. They also verify
cross-room projectile movement, distant-player rejection, overflow refusal, and
atomic rollback of damage, random state and allocations. The projectile oracle
explicitly enables the game's GBC flag, which controls torch interactions.
Run `Scripts/test_actors.py` and `Scripts/test_projectiles.py` after the core
build. CI runs them with the existing checks.

**Not yet integrated into live play:** no neighboring actor scheduler or
ownership handover is enabled in the app. The native continuation starts after
recoil and enemy damage; it does not implement those earlier enemy phases.
Other enemy AI, player weapons, drops/death/fall handlers, per-room sprite assets,
room events, persistence of new actors and transitions still require work.
Enemy-projectile contacts above are a tested subsystem, not a claim that all
combat works in the app. The widescreen switch remains a Debug scenery prototype.

The preceding published revision (b39a8218) passed the Linux checks, Debug
simulator build, Release device build and simulator launch on GitHub. This
does not replace gameplay validation on an actual iPhone.
