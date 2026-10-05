# Scheduled water

## Repository integration

- `SubChunk::_blocks` stores one byte per block; `Chunk` owns vertical subchunks,
  and `ChunkLoader` resolves world coordinates, caches columns and applies edits.
- `define.hpp` contains block IDs. `Water.hpp` extends those IDs for fluid states,
  supplies amount/depth conversions, surface heights and player current vectors.
- `Raycaster` placement/destruction goes through `setBlockOrQueue` / `setBlock`.
  Every actual `setBlock` change now wakes neighboring water, including removal
  and generated structures. Plants are replaceable; this game has no item drops.
- `StoneEngine::updateGameTick` calls the loader at 20 Hz. The water queue schedules
  deadlines five game ticks later (250 ms); it is independent of mesh publication.
- `WaterSimulation.hpp` contains the shared rules and coordinate-only deadline
  queue. `ChunkLoader` supplies cached reads and applies the resulting local edits.
  The test world runs the same solver and scheduler, not a second implementation.
- Generation and meshing use the existing `ThreadPool`. Water runs on the game
  thread under `_blockEditMutex`, also used for edits, LOD refinement and eviction.
  Subchunk access keeps the existing `_dataMutex` protection. No event stores a
  chunk pointer. Data readiness is independent of whether faces were published.
- `SubChunk_faces.cpp` derives corner geometry. Changes dirty the containing
  column and touching horizontal/diagonal columns through the existing dirty set;
  each column rebuild includes its vertical subchunks. Loading terrain also dirties
  adjacent surface samples. Existing transparent passes and greedy meshing remain.
- `Player::waterSurfaceAt` samples the same corners used for meshes and queries
  `waterCurrent`. Existing immersion weighting, collision, drag, swimming pulses,
  and tunable current strengths in `Swimming.hpp` remain in use.
- There is **no disk save/serialization format** in this repository. Fluid edits
  retain their exact state in modified columns, which cannot be evicted, just like
  player edits. Closing the game loses both. No persistence format was invented.

## State and rules

| Stored byte | Meaning | Amount | Falling |
| --- | --- | --- | --- |
| `W` | Source (generated or placed) | 8 | false |
| `1` ... `6` | Horizontal depth 1 ... 6 | 7 ... 2 | false |
| `8` | Horizontal depth 7 | 1 | false |
| `7` | Falling column (existing ID retained) | 8 | true |
| Air / non-water | No fluid | 0 | false |

Use `waterAmount`, `waterDistance` and `waterFromAmount`, not byte arithmetic.
Falling is separate because vertical distance must never consume horizontal
reach. A landing cell can feed seven more horizontal cells. Exposed height is
amount / 9, rounded to fifteenths for the renderer's existing four-bit vertices;
water above makes a shared corner full height. Adjacent surfaces use the same
four cells and rounding, including at chunk boundaries.

Existing sources never recompute or require support. Other cells first try to
convert to sources with two horizontal sources and known solid/source support;
then receive falling water from above; otherwise take the strongest horizontal
amount minus one, disappearing at zero. `setWaterSourceConversion(false)` disables
new source conversion (default true); it applies on subsequent water updates.

Only water cells tick. Empty cells are created by spreading from existing water,
not simply because they happen to neighbor water. This is necessary for routing.
Downward spread has priority; a pouring cell also spreads sideways only with at
least three horizontal source neighbors. When downward replacement is blocked,
a source can feed its banks, while flowing water spreads only off a non-hole.
Horizontal writes replace weaker water; an existing cell decays on its own tick.

For each passable non-source horizontal candidate, search for the nearest drop.
An immediate drop scores zero. Search up to four further steps, exclude immediate
backtracking, and take the minimum across all paths. Thus a straight drop five
blocks from the original cell can influence it; six blocks away cannot yet do so.
All directions tied for the lowest score win, including all valid directions when
no hole is found. The search avoids blindly forming a symmetric pool near cliffs.
Lookups are cached only for one event, including writes during that event.

The queue deduplicates each position while pending, retains its first deadline,
and processes at most 512 due positions per game tick in deadline/coordinate
order. New changes schedule five ticks later. Large active areas may slow under
that budget. Stable cells stop scheduling entirely. Neighbor changes wake only
affected water, so source removal recedes incrementally without a global scan.

Unavailable/coarse terrain is unknown, never air or solid support for creating a
source. An event touching unknown terrain defers its complete edit set, records
which columns it needs, and leaves the active queue. Full-resolution load/refinement
wakes those coordinates; no polling or force-loading occurs. A one-time scan of
new full-resolution columns seeds exposed/generated and border water. Interior
ocean sources do not need scheduled ticks. Deferred dependencies contain coordinates
only, so eviction cannot leave dangling fluid events.

## Currents and debug view

Currents sum horizontal amount gradients, including lower water across ledges,
then normalize. Falling cells beside blocking faces add a strong downward component.
Flat pools have no current; source edges can have one. The player's existing
strength/drag coefficients apply the vector independently of texture animation.

Press **F7** for a 17×17 textual slice at the player's feet. `S` is source, `F8`
is falling, `7` ... `1` are amounts, `.` is replaceable, `#` is blocking, and `?`
is unavailable. The center and Y coordinate appear above the grid; +X runs right
and +Z down. Fly/move to change the sampled slice. Toggle F7 again to hide it.
The mode is off by default and allocates its text renderer only when first enabled.

## Validation

`make test` runs the water rule tests, real chunk integration, both mesh suites,
swimming/current tests, raycast regression and mouse capture regression. It needs
`glslangValidator`, `xvfb-run`, and the existing GL/X11 development dependencies.
Individual targets are `test-water`, `test-water-integration`, `test-water-meshing`,
`test-flowing-water`, `test-swimming`, `test-raycast`, and `test-mouse-capture`.

The rule suite checks timing, seven-block diamonds, waterfalls and landing reach,
near/equal/out-of-range drops, walls, decay, 1×3 and 2×2 regeneration, disabled
conversion, floating sources, crossing streams, plants and equilibrium. The real
loader suite checks negative-coordinate chunk seams, unknown-neighbor deferral,
load wakeup, dirty meshes, modified-cache retention and eviction with queued work.

## Deliberate limits

This targets the specified modern Java rules, not bit-for-bit parity with a
particular release. Deterministic coordinate tie order and the 512-event budget
can differ from Java's scheduling order. Existing equal-weight surface corner
averaging is retained instead of Java's weighted corner sampling; heights remain
quantized and ocean reflections/fog retain the existing global sea plane. Swimming
constants remain project-specific. Boundary simulation pauses conservatively when
any required local/search data is unavailable. Remote edits do not wake all water
within the slope-search radius; as with neighbor-driven fluids, routing is evaluated
when a cell next receives an update.

There are no waterloggable/shared-fluid blocks or face collision shapes here.
`canWaterPass(read, from, to)` keeps both cells available for future face-specific
rules; current plants accept replacement, and other blocks block passage. Logs and
cactus have inset render geometry but remain blockers in this fluid model.
Buckets, item drops, mobs, boats, lava reactions, bubble columns, freezing, sponges,
and Nether evaporation have no corresponding mechanics and were not added.
