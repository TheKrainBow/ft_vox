#pragma once
#include "CaveWaterGenerator.hpp"
#include "Lava.hpp"

struct LavaSettings {
    float caveFrequency = 0.5f;
    int caveMinY = 8, caveMaxY = 180;
    float surfaceChance = 0.06f;
    int surfaceRegionSize = 256;
};
struct LavaPlacement { CaveWaterPos position; char block; };
class LavaGenerator {
public:
    using Height = std::function<int(int,int)>;
    using Allowed = std::function<bool(int,int)>;
    explicit LavaGenerator(uint64_t seed, LavaSettings settings = {});
    const LavaSettings& settings() const { return m_settings; }
    std::vector<CaveWaterPos> caves(CaveWaterPos min,CaveWaterPos max,
        const CaveWaterGenerator::Sample& sample,const CaveWaterGenerator& water, bool cacheFeatures = false) const;
    // Complete surface feature plans for this XZ box, including stone banks and
    // clearing the open cavity. Each subchunk applies only its own Y portion.
    // Only enable caching with immutable world geometry; custom samplers default off.
    std::vector<LavaPlacement> surface(CaveWaterPos min,CaveWaterPos max,
        const Height& height,const CaveWaterGenerator::Sample& sample,const Allowed& allowed,
        bool cacheFeatures = false) const;
private:
    mutable std::mutex m_surfaceMutex;
    mutable std::map<std::array<int,2>,std::shared_future<std::vector<LavaPlacement>>> m_surfaceFeatures;
    uint64_t m_seed;
    LavaSettings m_settings;
    CaveWaterGenerator m_caves;
};
