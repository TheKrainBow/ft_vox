# Compact cave generation

The feature replaces `CaveGenerator`, rather than adding a second terrain generator.
It changes only the stone/air decision. No block IDs, underground materials, fluids,
biomes, structures, or decorations were added. Existing overworld surface materials
and ocean generation remain in place.

## Integration and coordinates

- Chunks are 32×32 columns of 32³ subchunks, with byte block storage. World Y
  increases upward; bedrock is at Y=0. Generation starts at subchunk Y=0 and
  allocates up to the local maximum terrain height plus headroom.
- `NoiseGenerator::getHeight` blends continental, erosion and peak splines from
  existing 2D noise. Sea level is 111; peak spline scaling can reach about 630.
  There is no fixed allocated upper world boundary. Cave defaults use Y=1..640
  with smooth protection at both ends; terrain above that remains solid.
- `Chunk` prepares the actual integer surface height, cave column data and surface
  entrance decisions once, sharing them across vertical subchunks. `loadHeight`
  writes local stone/bedrock under one storage lock rather than routing and locking
  each voxel. The old synthetic cave surface
  and `maxHeight + 40` offset are gone. Y=0 is still assigned bedrock first.
- `Chunk::loadBlocks` runs subchunk generation on the thread pool, calling
  `loadHeight` before `loadBiome`. Surface processing skips entrance mouths, and
  desert caps preserve carved air instead of sealing slanted openings.
- Cave decisions need no loaded neighbors or random draws. Column initialization
  uses `call_once`; optional noise interpolation uses bounded worker-local caches.
  Meshing still uses existing subchunk/chunk neighbor access and greedy face
  construction. No meshing or scheduling changes were needed.
- The existing LOD policy is preserved: caves are generated only at resolution 1.
  Distant coarse terrain is solid. LOD transitions are not new cave seams.

## Fields and seeding

All fields use the existing signed `Noise3DGenerator` Perlin implementation.
A SplitMix64-style hash of `worldSeed + fieldIndex` supplies independent 32-bit
permutation seeds. Each field also has a different fractional phase to avoid
shared Perlin lattice zeros at the origin. Generators are initialized once and
sampled read-only. Determinism holds across chunk order and threads in the same
build; the existing library's `default_random_engine` / `shuffle` do not promise
identical worlds across different C++ standard library implementations. Existing
saved/generated terrain will not match the old cave layout.

- **Cheese:** anisotropic low-frequency noise plus one small detail octave.
  A separate slow region field changes its threshold, and depth slightly lowers
  the threshold. Two 2D fields retain scattered vertical stone columns whose
  radii vary gently with height. Overlapping tunnels may cut through a pillar.
- **Spaghetti:** `max(abs(A), abs(B)) < width`. Each zero field describes a
  surface; two surfaces intersect along winding curves, and a finite width makes
  tubes. Independent slow noise varies width; small roughness perturbs it.
  This common field is evaluated first and supplies the exploration backbone.
- **Noodles:** the same intersection technique at higher frequency and smaller
  width, restricted by a smoothly fading regional mask. They are uncommon.
- **Entrances:** rare 2D regions allow spaghetti curves to reach the surface,
  including the topmost stone voxel. Entrance influence fades into ordinary
  tunnels with depth, preventing a flat cutoff. Heights below 115 are excluded
  to leave existing oceans and beaches sealed. Connectivity is emergent: not
  every mouth or cave is guaranteed to connect to a large network.
- Surface, bottom, upper-limit, and depth modifiers are smooth. Ordinary caves
  cannot touch the top three blocks. Depth gradually expands the backbone and
  makes chambers more likely. Optional low-frequency XYZ warp costs three extra
  samples, so its default strength is zero.

No per-voxel allocation, generator construction, surface query, graph search, or
mutable statistics are used. Cheap range/surface tests, conservative field bounds,
conditional detail/roughness samples, and shared entrance/pillar fields limit work.
The field bounds use 2 (two gradient offsets of magnitude at most 1), so they do
not depend on an assumed tighter Perlin range. Surface-only biome cap work is
skipped in unrelated vertical layers for plains, forest, mountain and snow.
No full Minecraft noise router, aquifer, legacy carver, or biome system is reproduced.

## Optional coarse noise sampling

`CaveGenerationSettings::noiseSampleStep` defaults to **1**, preserving the original
voxel layout. Set it to **2** or **4** to interpolate signed cheese, cheese-region,
tunnel-thickness and noodle-region noise on a world-aligned XYZ grid. Other values
normalize to 1, 2 or 4. Spaghetti/noodle curves, detail, roughness, pillars,
entrances, protection and liquid containment still use per-block decisions.
Never copy one stone/air decision across a whole cell.

Interpolation is central to `CaveGenerator`, so stone carving, surface checks and
water/lava geometry queries all agree, including unloaded neighbors and negative
coordinates. Each worker lazily caches eight 32-block tiles of lattice vertices,
sharing corner samples across adjacent cells and endpoints across vertical
segments. This uses about 95 KiB at step 4, or up to 634 KiB after using step 2.
Generator IDs isolate cached values across seeds, settings and reused addresses.
Chunk column data uses about 21 KiB per full-resolution chunk, included in chunk
memory accounting.

The optional final `ChunkLoader` constructor argument accepts these settings,
after water and lava settings:

```cpp
CaveGenerationSettings caves;
caves.noiseSampleStep = 4;
ChunkLoader loader(seed, camera, pool, chrono, &running, drawMutex,
                   solids, transparent, {}, {}, caves);
```

This mode changes chamber boundaries and may change accepted fluid sites. It is
experimental: fewer noise calls do not guarantee faster generation because vertex
lookups and interpolation also cost CPU. Step 4 benefits from sharing more work
per cell; step 2 can still be slower than exact sampling. Keep step 1 to retain
the existing layout, and benchmark before changing it. The exact optimizations
apply in every mode.

## Tuning

Defaults live in `includes/CaveGenerator.hpp`, in `CaveGenerationSettings`.
Supply modified settings as `CaveGenerator(seed, settings)` at the loader.
Frequencies are inverse blocks; thresholds are in signed Perlin units. Keep
protection heights and fade intervals positive, and maxY greater than minY.

| Desired change | Controls |
| --- | --- |
| Less overall air | Raise `cheeseThreshold`; lower `spaghettiThickness` and `noodleThickness`. There is deliberately no single density multiplier hiding different effects. |
| More/fewer chambers | Lower/raise `cheeseThreshold` (0.53). `cheeseThresholdVariation` (0.22) increases regional contrast; `cheeseDepthBonus` (0.09) favors deeper chambers. |
| Larger/smaller chambers | Lower/raise `cheeseScaleXZ` (0.032). Adjust `cheeseScaleY` (0.052) independently for taller/flatter spaces. Expect mostly 10–30 block rooms, with larger merged spaces; these are noise targets, not bounds. |
| Wider/narrower main tunnels | `spaghettiThickness` (0.105), variation (0.045), and depth bonus (0.025). Width is a noise threshold, not a radius in blocks; the intended main scale is roughly 3–7 blocks. |
| More/fewer bends | Raise/lower `spaghettiScaleXZ` (0.032) and `spaghettiScaleY` (0.042). Raising frequency also reduces physical width unless thickness rises. |
| More/fewer noodles | Lower/raise `noodleRegionThreshold` (0.18). `noodleRegionScale` (0.012) sets cluster size. Thickness 0.070 and XZ/Y scales 0.065/0.075 target 1–3 blocks; tangencies can be tighter. |
| More/fewer entrances | Lower/raise `entranceThreshold` (0.43). `entranceScale` (0.014) changes region spacing, and `entranceThickness` (0.13) changes mouth width. |
| More near-surface stone | Increase `surfaceProtectionDepth` (14). Entrances have their own depth limit (26). Keep entrance depth greater than surface protection. |
| More/fewer pillars | Raise/lower `pillarThickness` (0.085); zero with zero variation disables them. |
| Gentle additional bends | Try `warpStrength` around 1–2 blocks; default 0 avoids extra sample cost. |

After tuning, run the region test with several seeds. Statistics count the first
winning field (entrance, spaghetti, cheese, noodle), so overlaps belong to only
one class. A long run in a slice may follow a tunnel longitudinally and does not
measure its diameter.

## Tests and inspection

```
make -j4
make test-caves test-caves-integration
make test-cave-sampling
/tmp/ft_vox-caves-test /tmp/caves-1337.ppm 1337
/tmp/ft_vox-caves-test /tmp/caves-step4.ppm 1337 4
```

The region test emits a PPM vertical slice at Z=17 over X=-128..127, Y=160..1.
Colors: dark = stone, orange = cheese, blue = spaghetti, purple = noodle,
green = entrance. These are debug image colors, never world block types.
It also prints per-class percentages, longest sampled X runs, surface mouth
counts, and adjacent air crossings at chunk borders. The fixed surface at Y=160
isolates geometry from terrain; the integration test uses real terrain maps.

Validated locally:

- Optimized build with `-Wall -Wextra -Werror`.
- Same-seed repeatability, different seeds, concurrent sampling, negative
  coordinates, all three cave scales, protected surface/bottom/top limits,
  sparse entrances, and air crossings at chunk borders.
- Nine neighboring production chunks, generated twice through the worker pool:
  1,266,237 underground stone/air checks, 5,948 adjacent border crossings.
- 335 surface openings preserved across plains, desert, mountain, snowy, and
  forest surface passes (with an intentionally permissive test entrance mask).
- AddressSanitizer and UndefinedBehaviorSanitizer on the standalone cave suite.
- Existing water, water integration, water meshing, flowing-water meshing,
  swimming and below-bedrock raycast tests, including shader validation.
- `make test` cannot complete the mouse-capture test because `xvfb-run` is absent.
  Its executable compiles; other targets were run explicitly.
- The application starts under the available display (10-second smoke run).
  The generated debug slice was visually inspected: distinct chambers, winding
  tunnels, narrow branches and large solid regions. A complete interactive
  first-person/no-clip cave tour has **not** been performed; traversal quality,
  small isolated pockets, and actual diameter distributions remain qualitative
  tuning limits rather than guarantees.

Seed 42, 256×256×160 flat-surface sample: 86.03% stone, 3.42% cheese,
10.22% spaghetti, 0.29% noodle, 0.047% entrance; 187/65,536 surface columns
open (0.285%). Chamber X runs reached 55 blocks, including merged spaces.
Seeds 1337 and 98765 also passed: all three samples retained 85.75–86.62%
stone, with surface openings covering 0.285–0.992% of columns.

Performance comparison used 10,485,760 decisions at the same coordinates,
optimized builds, seed 42, surface 160 (old code received its original +40 offset).
The old predicate took approximately 0.74 seconds; the new cached-column path
approximately 1.12 seconds: **about 50% more cave-decision CPU**, or roughly
5.8 ms additional work per 32×32×160 chunk column. This is a meaningful cost,
not a whole-frame or full chunk-generation benchmark. The production integration
suite, including repeated generation and assertions, took about 0.33 seconds.
The old layout carved about 33% of this sample versus the new 14%, so different
mesh costs may offset or increase total loading impact. Keep warp off unless
its visual benefit justifies its added cost.

Optimization validation: an external differential harness compared 21,123,072
exact classifications against the pre-optimization implementation, including
three seeds, warping, permissive entrances and negative threshold variations;
all matched. The sampling suite checks steps 2/4, negative coordinates, agreement
at grid nodes, traversal/thread independence, cache reuse with replacement
generators and preservation of entrances/narrow branches. Across its three
128x64x161 volumes, step 4 changed about 0.50–0.56% of stone/air decisions and
retained 98% or more of the original noodle voxel locations as air.
Production containment/separation tests run independently at steps 1, 2 and 4.

A temporary generation-only harness (seed 42, 81 real chunks, four workers,
prebuilt terrain maps; no meshing/rendering) measured the following in one paired
run. Warm results average two repeated passes:

| Wall time for 81 chunks | Before | Optimized, step 1 |
| --- | --- | --- |
| Cold water/lava feature caches | 995 ms | 900 ms |
| Warm water/lava feature caches | 941 ms | 836 ms |

That is about 10–11% less generation wall time in this sample, with unchanged
blocks. Full block snapshots also matched the original SubChunk path for steps
1/4 at LOD resolutions 1, 2, 3, 4 and 8. Step 4 did not consistently improve the
whole-generation timing, and was slower in isolated cave scans; it remains opt-in.
These are local CPU measurements, not frame-rate guarantees.

## Sampling cache investigation

The first interpolation cache stored eight corners separately for each cell.
That repeated noise calls for vertices shared with adjacent cells, and its 256
hashed slots per field suffered collisions. A miss cost eight evaluations, so
step 2 could actually compute **more** noise than exact sampling. Runtime divides,
cache checks and interpolation then added cost to every eligible voxel.

A counted seed-42 probe (128x128x160, traversed in 32-block vertical layers,
including column preparation) measured:

| Step | Original cell cache: Perlin calls | Shared vertex tiles: Perlin calls |
| --- | ---: | ---: |
| 1 | 13,835,364 | 13,835,364 |
| 2 | 20,969,766 | 8,404,236 |
| 4 | 9,597,617 | 7,364,962 |

At step 4 the four interpolated fields fell from about 6.64 million calls to
0.205 million. Total calls only fell by 47% because spaghetti, noodle curves,
detail, roughness and other exact fields still run. A grid spacing of four on XYZ
therefore does not make the entire cave decision 64 times cheaper—or four times
cheaper. Classifier logic and per-block containment also remain.

The revised cache evaluates each requested vertex once per retained tile, lazily
fills only needed fields, reuses vertical endpoints, and uses constant reciprocals
instead of per-voxel divisions. In a rotating five-run chunk-order benchmark,
step 4 improved from about 246 ms to 225 ms for 2.62 million decisions, versus
249 ms for exact sampling (about 10% faster). Step 2 improved from about 437 ms to
270 ms, but still cost more than exact sampling. These local timings are workload
dependent. Traversing large regions column-by-column can exceed the tile working
set; the benchmark follows production's 32-cubed subchunk tasks instead.

Run `make bench-cave-sampling` to compare modes with columns prepared once,
five rotated runs and median times, without assertions or image-writing overhead.
One local run of that target measured 235 ms (step 1), 255 ms (step 2) and 212 ms
(step 4). In the generation-only 81-chunk/four-worker harness, step 4 averaged
768 ms with warm feature caches versus 818 ms for exact sampling (about 6% less
wall time); the additional biome/placement work limits the overall improvement.
The vertex-cache replacement also matched 9,750,108 classifications from the
previous interpolated implementation across steps 2/4, three seeds, negative
coordinates and warp strengths 0/2; it changes sampling cost, not geometry.
