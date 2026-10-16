# Underground cave water

`CaveWaterGenerator` selects sparse, seeded features in world-space regions after
cave carving. `SubChunk::loadBiome` applies the resulting water after the existing
surface pass, before mesh generation. Surface terrain, biome water and sea level
calculations are unchanged. Cave water is generated only at full resolution, like
caves, and is rebuilt when a coarse chunk returns to full resolution.

The generator samples original cave geometry and a private seeded height sampler,
including outside the output chunk. It never reads unloaded neighbors or modifies
them. Every intersecting subchunk uses the complete feature plan and writes
only its own portion, giving identical results across negative coordinates,
horizontal/vertical borders and generation order. Centers are inset from feature
region edges so independent features cannot remove each other’s containment. Column and voxel samples are
cached for the duration of one generation call. Production generation also shares
a bounded cache of 2,048 complete feature plans (including rejected candidates).
Concurrent subchunks share an in-flight result rather than repeating noise and
containment searches. Custom geometry samplers remain uncached unless explicitly
opted in.
Cached plans are read without copying their entire voxel vectors. Temporary
column/voxel lookups use hash tables, and local columns reuse the chunk's immutable
cave data. Optional cave noise interpolation is shared with stone generation;
containment and placement still validate individual voxels.

Pools fill actual connected cave depressions. Small candidates use radii 2–5 and
depths 1–3; 60% of candidates use larger radii 8–16 and depths 3–6. Large pools
must actually span at least 14 blocks on one horizontal axis, rather than falling
back to a tiny puddle with a large search radius. A fill
that reaches an open side, falls below its supported starting floor, touches
protected terrain or exceeds its voxel budget is rejected. Shallower levels are
tried before abandoning a basin. This avoids artificial circular water edges and
ensures generated sources cannot drain into the wider cave through a lower hole.

Wall pockets carve distorted, tapered cavities in stone near a cave wall.
Small variants have radii 2–3 and depths 1–3; large variants use radii 5–8 and
depths 3–6. Large pockets must span at least eight blocks. All faces below the local
water line must have solid containment. The cave exposes a dry opening above
the pocket's rim. Pockets with wet side openings are rejected because the game's
source-water simulation would turn those openings into unbounded waterfalls.

Wall springs carve exactly one source block recessed in an exposed cave wall.
Five faces remain solid; one horizontal face opens into cave air. The outlet must
have a landing floor 2–16 blocks below it, with no protected terrain along that
drop. The existing water scheduler wakes the exposed source when its chunk loads,
then spills flowing water out of the hole and down to the floor. Generation writes
only the source; it does not prefill the waterfall or create a water plane.

Configure `CaveWaterSettings` using the optional `ChunkLoader` constructor argument
after the staging queues (followed by lava and cave settings). Settings are immutable
while workers run:

| Parameter | Default | Meaning |
| --- | --- | --- |
| `caveWaterFrequency` | 1.4 | Expected candidate attempts per feature region; 40% more than before |
| `regionSize` | 48 | Region side length in blocks |
| `caveWaterMinY` / `caveWaterMaxY` | 12 / 180 | Underground range; maximum exclusive |
| `minimumUndergroundDepth` | 24 | Required depth below local terrain |
| `caveWaterPoolChance` / `caveWaterPocketChance` | 0.75 / 0.25 | Relative type weights |
| `caveWaterSpringChance` | 0.25 | Relative weight for single-source wall springs |
| `caveWaterMinRadius` / `caveWaterMaxRadius` | 2 / 16 | Pool radius limits |
| `largePoolChance` | 0.6 | Large variant probability for pool/pocket attempts |
| `smallPoolMaxRadius` / `largePoolMinRadius` | 5 / 8 | Small/large natural basin ranges |
| `maxDepth` | 6 | Maximum large pool/pocket depth |
| `maxVisitedVoxels` | 8192 | Total accepted fluid volume budget per region |
| `deepFrequencyMultiplier` / `upperFrequencyMultiplier` | 1 / 1 | Uniform water frequency with elevation |

Frequency zero disables the feature. Geometry validation rejects many candidates;
the type weights describe attempts, not guaranteed successful feature ratios.
Most cave space stays dry. Frequency 1.4 means one candidate plus a 40% chance of
a second independent candidate, using the same type weights for pools and springs.
Up to four candidates are supported per region; accepted features reserve their
wet volume and supporting faces so later candidates cannot break containment.
Actual placement counts depend on geometry and may differ from the attempt ratio.
Each candidate searches a bounded 13×17×13 neighborhood on a two-block horizontal
grid, and never floods an entire connected cave. Radius is capped at 24, depth at
8, region size at 32–256 (raised if necessary to fit the radius) and the regional
voxel budget at 16–16384 even with custom settings. Height/depth protection also rejects direct
vertical surface openings. Water uses the existing `WATER` source block and the
existing rendering, swimming and fluid scheduling.

Define `DEBUG_CAVE_WATER` to log candidate coordinates, type, radius, accepted
voxel count and rejection. A feature may be logged by multiple intersecting
subchunks. Logging is disabled by default. F7's existing water slice can inspect
placed sources in-game.

`make test-cave-water` checks synthetic basins, wall pockets and single-source
springs, containment, protected geometry, seed repeatability, output split across
chunk borders, and spring flow to the ground using the production water solver.
`make test-caves-integration` checks actual generated terrain against a disabled
baseline, including unchanged surface blocks and ocean water.
`make test-fluid-generation` isolates frequency and pool sizes with synthetic
geometry: its fixed sample produced 122→165 pools and 220→311 springs with the
frequency change, while retaining small pools and finding large 2,500-block pools.
