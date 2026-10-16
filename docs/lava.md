# Lava

`LAVA` (`V`) is a separate liquid block. It uses water's normal/ripple texture and
liquid face geometry, with orange near faces fading to dark red over 5–50 blocks.
It remains emissive in dark caves. Its alpha is `1 - (1 - waterAlpha) * 0.1`, so
water's 80–90% opacity becomes 98–99% lava opacity. Atlas-free material IDs 15–23
keep lava separate from water and preserve greedy meshing, including seven flow
heights. Flowing lava uses shared sloped corner heights like water; source
pools keep flat tops, and covered flows become full height. Shaped lava stores
its material in draw metadata (bit 3), retaining four-bit corner precision and
32-bit instances. Internal lava faces
are culled; the top remains visible under non-lava ceilings.

## Generation

`LavaGenerator` reuses the bounded cave pool/pocket/spring analysis with a separate
seed stream. Defaults are Y=4..179, small pool radii 2–5, large radii 8–16,
small depths 1–3 and large depths 3–6. Wall pockets have small radii 2–3 and
large radii 5–8. Large variants account for 60% of pool/pocket attempts; small
variants remain. Pool/pocket/spring relative weights are 0.65/0.35/0.25.
Wall springs place one source recessed into a cave wall, with a 2–16 block drop
to a landing; the existing solver produces its outflow and fall.

Candidate frequency decreases strongly with elevation. For a region's midpoint
`t = clamp((y - caveMinY)/(caveMaxY - caveMinY), 0, 1)`, the expected number of
attempts is `min(4, caveFrequency * (0.15 + (4 - 0.15)*(1-t)^2))`. Thus the lowest
48³ regions get roughly three attempts, while the highest get about 0.17.
The profile is world-space and independent of the requested output box.
It never creates a global lava level. Water placement runs first. Lava
reconstructs nearby water reservations from world geometry, keeps a two-block
buffer, and also excludes water-spring runoff corridors and their landing reach.
Lava springs additionally check their potential fall/spread corridor against those
reservations. Reservations cover complete lava feature regions, so cached plans
remain independent of which subchunk requests them first.
Chunk order, negative coordinates and unloaded neighbors do not change placement.

Occasional surface pools use independent 192² regions with a 12% candidate chance,
radii 3–6 and one/two blocks of lava depth. Sites must be above sea level +12,
have no more than three blocks of relief, and have solid containment. Forest,
snow, ocean and beach locations are rejected. A stone floor and complete two-block
stone bank surround the actual shoreline, including its corners. Lava columns are
cleared to the sky; the two-block bank also keeps nearby tree canopies out of them.
The surface plan is stamped before biome caps/decorations; reserved
columns skip those writes. Failed sites are rejected rather than cutting large
craters. Ocean generation is unchanged.

The optional `ChunkLoader` constructor argument after water settings accepts
`LavaSettings`: `caveFrequency`, `caveMinY`, `caveMaxY`, `surfaceChance`,
`surfaceRegionSize`, `deepFrequencyMultiplier` (4), `upperFrequencyMultiplier`
(0.15) and `springChance` (relative weight 0.25).
Zero frequencies disable either type. Generated caves use full resolution;
surface plans are also sampled at the current chunk LOD.
Production generation caches complete cave water/lava features across subchunks.
Water reservations are constructed lazily when a new lava feature needs geometry;
reusing a lava plan needs no additional cave noise or reservation analysis.
Surface features also share a bounded cache of 512 complete plans, including
rejections, across subchunks and worker threads. Custom height/geometry/biome
samplers remain uncached unless explicitly opted in. Water-separation reservations
use a hash set; containment and the separation distance are unchanged.

## Light and immersion

Each rendered/exposed lava face registers its center as an orange light emitter.
A sparse world-space hash grid provides nearby emitter lookup on the GPU. Light
fades smoothly to zero at four blocks from the face center, with overlapping
sources taking the strongest contribution rather than growing without bound.
The field rebuilds after lava/source-neighbor edits, full-resolution chunk loads
or eviction; it is reused between changes and does not scan terrain each frame.
Face collection and field construction run on a dedicated worker. Pending
revisions coalesce, identical faces reuse the field, and rendering keeps the last
completed lighting until the replacement is ready. Only GPU uploads run on the
main thread. Water ticks defer without consuming queued events while generation
holds the edit lock, then resume on the next available tick.
Terrain, leaves, plants and water receive the light. Local lava lighting is radial;
it does not add a second voxel raytracing/shadow system for point lights.

Lava is passable and excluded from ground/standing queries. With gravity enabled,
horizontal swimming and vertical swim integration run at exactly half the water
speed, including sprinting. Flying retains its ordinary speed. Eye immersion uses
the same sloped flow and flat source/full heights as the lava mesher and follows the local column. Dense orange/red fog and
a very opaque final immersion filter appear in lava and clear when the eyes leave
it. Water keeps its blue/green filter.

Lava uses the same downward routing, seven horizontal flow levels and receding
flows as water, with a 10-game-tick delay (500 ms), twice water's delay, so lava
flows half as fast.
Lava does not create new sources from neighboring sources. Each fluid blocks the
other; water does not use lava as support for new sources. Edits wake flows, chunk
load events wake waiting border flows, and exposed flowing faces emit light too.
There is no health/damage or lava/water reaction system. Source edits follow the
game's existing modified-chunk retention rules.
F7's liquid slice marks lava as `L`; block picking and the debug block list include
lava.

## Validation

`make test-lava` checks deterministic caves, two-block water separation, contained
surface pools/two-block stone banks/open sky, wall-spring outflow and falls,
chunk borders (including reverse cached generation), surface rarity, opacity and
light bounds. `make test-fluid-generation` checks small/large pools and the depth
profile against identical geometry at different elevations; its fixed sample
found lava in 105 bottom-layer regions versus six upper-layer regions.
`make test-caves-integration` checks real lava/water terrain and rare surface
candidate validation against actual heights/biomes. `make test-water-integration`
checks actual eye immersion, half-speed horizontal/vertical swimming, fly mode,
and light-emitter invalidation after placing/removing lava and closing a face.
GPU suites (`test-player-light`, `test-water-lighting`) render the real shaders to
check orange four-block emission, its removal/range cutoff, lava distance shading,
98–99% opacity and dense fog. Existing water, meshing, raycast and swimming targets
remain part of the regression suite.
`make test-lava-worker` verifies nonblocking rendering-side polling while world
collection is held, coalesced revisions, field reuse and light removal.
`make test-lava-flow` checks slower deadlines, seven-level spread, falling/decay,
water separation and no source conversion. `make test-display-edits` checks
mesh publication while terrain workers are saturated, replacement of stale
snapshots and nonblocking queued edits while the world-edit lock is held.
