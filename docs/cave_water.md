# Underground cave water

`CaveWaterGenerator` selects sparse, seeded features in world-space regions after
cave carving. `SubChunk::loadBiome` applies the resulting water after the existing
surface pass, before mesh generation. Surface terrain, biome water and sea level
calculations are unchanged. Cave water is generated only at full resolution, like
caves, and is rebuilt when a coarse chunk returns to full resolution.

The generator samples original cave geometry and a private seeded height sampler,
including outside the output chunk. It never reads unloaded neighbors or modifies
them. Every intersecting subchunk reconstructs the complete feature and writes
only its own portion, giving identical results across negative coordinates,
horizontal/vertical borders and generation order. Centers are inset from feature
region edges so independent features cannot remove each other’s containment. Column and voxel samples are
cached for the duration of one generation call; no shared mutable cache is used.

Pools fill actual connected cave depressions, usually 1–4 blocks deep with radii
2–5. Fifteen percent of pool candidates can use larger radii, up to 10. A fill
that reaches an open side, falls below its supported starting floor, touches
protected terrain or exceeds its voxel budget is rejected. Shallower levels are
tried before abandoning a basin. This avoids artificial circular water edges and
ensures generated sources cannot drain into the wider cave through a lower hole.

Wall pockets carve small distorted, tapered cavities in stone near a cave wall.
They are 1–4 blocks deep with horizontal radii 2–3. All faces below the local
water line must have solid containment. The cave exposes a dry opening above
the pocket's rim. Pockets with wet side openings are rejected because the game's
source-water simulation would turn those openings into unbounded waterfalls.

Wall springs carve exactly one source block recessed in an exposed cave wall.
Five faces remain solid; one horizontal face opens into cave air. The outlet must
have a landing floor 2–16 blocks below it, with no protected terrain along that
drop. The existing water scheduler wakes the exposed source when its chunk loads,
then spills flowing water out of the hole and down to the floor. Generation writes
only the source; it does not prefill the waterfall or create a water plane.

Configure `CaveWaterSettings` using the optional final `ChunkLoader` constructor
argument. Settings are immutable while workers run:

| Parameter | Default | Meaning |
| --- | --- | --- |
| `caveWaterFrequency` | 1.0 | Candidate probability per feature region |
| `regionSize` | 48 | Region side length in blocks |
| `caveWaterMinY` / `caveWaterMaxY` | 12 / 180 | Underground range; maximum exclusive |
| `minimumUndergroundDepth` | 24 | Required depth below local terrain |
| `caveWaterPoolChance` / `caveWaterPocketChance` | 0.75 / 0.25 | Relative type weights |
| `caveWaterSpringChance` | 0.25 | Relative weight for single-source wall springs |
| `caveWaterMinRadius` / `caveWaterMaxRadius` | 2 / 10 | Pool radius limits |
| `maxVisitedVoxels` | 2048 | Maximum accepted feature volume |

Frequency zero disables the feature. Geometry validation rejects many candidates;
the type weights describe attempts, not guaranteed successful feature ratios.
Most cave space stays dry. Regions choose one feature at most, search a bounded
13×17×13 neighborhood on a two-block horizontal grid, and never flood an entire
connected cave. Radius is capped at 12, region size at 32–256 (raised if necessary to fit the radius) and the voxel budget
at 16–4096 even with custom settings. Height/depth protection also rejects direct
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
