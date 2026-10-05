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
- `SubChunk::loadHeight` caches the actual integer surface height and cave column
  data, then evaluates caves at world coordinates. The old synthetic cave surface
  and `maxHeight + 40` offset are gone. Y=0 is still assigned bedrock first.
- `Chunk::loadBlocks` runs subchunk generation on the thread pool, calling
  `loadHeight` before `loadBiome`. Surface processing skips entrance mouths, and
  desert caps preserve carved air instead of sealing slanted openings.
- Cave generation needs no neighbors, mutable caches, locks, or random draws.
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

No per-voxel allocation, generator construction, surface query, interpolation
cache, graph search, or mutable statistics are used. The engine had no coarse 3D
noise interpolation to reuse. Cheap range/surface tests, short-circuit decisions,
conditional detail/roughness samples, and column-cached entrance/pillar fields
limit work. No full Minecraft noise router, aquifer, legacy carver, or biome
system is reproduced.

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
/tmp/ft_vox-caves-test /tmp/caves-1337.ppm 1337
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
