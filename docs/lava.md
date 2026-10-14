# Lava

`LAVA` (`V`) is a separate liquid block. It uses water's normal/ripple texture and
liquid face geometry, with orange near faces fading to dark red over 5–50 blocks.
It remains emissive in dark caves. Its alpha is `1 - (1 - waterAlpha) * 0.1`, so
water's 80–90% opacity becomes 98–99% lava opacity. Atlas-free material IDs 15/16
keep lava separate from water and preserve greedy meshing. Internal lava faces
are culled; the top remains visible under non-lava ceilings.

## Generation

`LavaGenerator` reuses the bounded cave basin/pocket geometry analysis with a
separate seed stream. Defaults are a 50% candidate chance per 48³ region,
Y=8..179, radii 2–6, and pool/pocket weights of 65%/35%. It never creates a global
lava level. Water placement runs first and retains its existing rules. Lava
reconstructs nearby water reservations from world geometry, keeps a two-block
buffer, and also excludes water-spring runoff corridors and their landing reach.
Chunk order, negative coordinates and unloaded neighbors do not change placement.

Rare surface pools use independent 256² regions with a 6% candidate chance,
radii 3–5 and one/two blocks of lava depth. Sites must be above sea level +12,
have no more than three blocks of relief, and have solid containment. Forest,
snow, ocean and beach locations are rejected. A stone floor/bank surrounds the
cavity. The surface plan is stamped before biome caps/decorations; reserved
columns skip those writes. Failed sites are rejected rather than cutting large
craters. Ocean generation is unchanged.

The optional final `ChunkLoader` constructor argument accepts `LavaSettings`:
`caveFrequency`, `caveMinY`, `caveMaxY`, `surfaceChance`, `surfaceRegionSize`.
Zero frequencies disable either type. Generated caves use full resolution;
surface plans are also sampled at the current chunk LOD.

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
the same flat source/full heights as the lava mesher and follows the local column. Dense orange/red fog and
a very opaque final immersion filter appear in lava and clear when the eyes leave
it. Water keeps its blue/green filter.

Lava pools are static fluid sources. The existing water simulation treats lava
as a blocker, does not replace it, and does not use it as solid support for new
water sources. There is no health/damage or lava/water reaction system in this
change. Source edits follow the game's existing modified-chunk retention rules.
F7's liquid slice marks lava as `L`; block picking and the debug block list include
lava.

## Validation

`make test-lava` checks deterministic caves, two-block water separation, contained
surface pools/stone banks, chunk borders, surface rarity, opacity and light bounds.
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
