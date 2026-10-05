#pragma once
#include "Water.hpp"
#include <cstdint>
#include <map>
#include <set>
#include <vector>

using WaterPos = std::array<int, 3>;
inline WaterPos waterOffset(WaterPos p, int x, int y, int z) {
    return {p[0] + x, p[1] + y, p[2] + z};
}
constexpr std::array<WaterPos, 4> WATER_SIDES{{{1,0,0}, {-1,0,0}, {0,0,1}, {0,0,-1}}};
inline WaterPos waterSide(WaterPos p, int d) {
    return waterOffset(p, WATER_SIDES[d][0], 0, WATER_SIDES[d][2]);
}

// Earliest pending deadline wins. The ordered set provides deterministic ties,
// bounded work and no pointers to chunk storage. Owners provide synchronization.
class WaterUpdateQueue {
    uint64_t _tick = 0;
    std::set<std::pair<uint64_t, WaterPos>> _events;
    std::set<WaterPos> _pending;
public:
    void schedule(WaterPos p) {
        if (p[1] > 0 && _pending.insert(p).second)
            _events.insert({_tick + WATER_TICK_DELAY, p});
    }
    std::vector<WaterPos> advance(size_t budget = 512) {
        ++_tick;
        std::vector<WaterPos> result;
        while (result.size() < budget && !_events.empty() && _events.begin()->first <= _tick) {
            auto p = _events.begin()->second;
            _events.erase(_events.begin());
            _pending.erase(p);
            result.push_back(p);
        }
        return result;
    }
    size_t size() const { return _events.size(); }
};

// The current block set has full blockers and replaceable decorations, no
// waterlogging. Keep both faces/positions in this API for future partial blocks.
template<class Read>
bool canWaterPass(Read& read, WaterPos from, WaterPos to) {
    return waterReplaceable(read(from)) && waterReplaceable(read(to));
}

template<class Read>
char computeWaterStateAt(Read& read, WaterPos p, bool sourceConversion = true) {
    char current = read(p);
    if (!waterReplaceable(current) || current == WATER) return current;
    int sources = 0, amount = 0;
    for (int d = 0; d < 4; ++d) {
        auto q = waterSide(p, d);
        if (!canWaterPass(read, q, p)) continue;
        char b = read(q);
        sources += b == WATER;
        amount = std::max(amount, waterAmount(b));
    }
    if (sourceConversion && sources >= 2 && waterSupport(read(waterOffset(p, 0,-1,0))))
        return WATER;
    auto above = waterOffset(p, 0,1,0);
    if (isWater(read(above)) && canWaterPass(read, above, p)) return WATER_FALLING;
    return waterFromAmount(amount - 1);
}

template<class Read>
bool waterHole(Read& read, WaterPos p) {
    // Compatible source water below counts as a hole for routing, even though
    // a downward write must preserve that source.
    return canWaterPass(read, p, waterOffset(p, 0,-1,0));
}

template<class Read>
int findWaterSlope(Read& read, WaterPos p, int back, int depth) {
    int best = 1000;
    for (int d = 0; d < 4; ++d) {
        if (d == back) continue;
        auto q = waterSide(p, d);
        if (!canWaterPass(read, p, q) || read(q) == WATER) continue;
        if (waterHole(read, q)) best = std::min(best, depth);
        else if (depth < WATER_SLOPE_DISTANCE)
            best = std::min(best, findWaterSlope(read, q, d ^ 1, depth + 1));
    }
    return best;
}

template<class Read>
std::array<bool, 4> waterSpreadDirections(Read& read, WaterPos p) {
    std::array<int, 4> scores{{1001,1001,1001,1001}};
    int best = 1000;
    for (int d = 0; d < 4; ++d) {
        auto q = waterSide(p, d);
        if (!canWaterPass(read, p, q) || read(q) == WATER) continue;
        scores[d] = waterHole(read, q) ? 0 : findWaterSlope(read, q, d ^ 1, 1);
        best = std::min(best, scores[d]);
    }
    return {scores[0] == best, scores[1] == best, scores[2] == best, scores[3] == best};
}

// Only existing water ticks. Empty cells are created by selected spread routes,
// never by indiscriminately recomputing every empty neighbor of a source.
template<class Read, class Write>
void processWaterTick(Read& read, Write write, WaterPos p, bool sourceConversion = true) {
    char current = read(p);
    if (!isWater(current)) return;
    if (current != WATER) {
        char next = computeWaterStateAt(read, p, sourceConversion);
        if (next != current) write(p, next);
        current = next;
        if (!isWater(current)) return;
    }
    auto below = waterOffset(p, 0,-1,0);
    bool down = canWaterPass(read, p, below) && read(below) != WATER;
    if (down) {
        char next = computeWaterStateAt(read, below, sourceConversion);
        if (next != read(below)) write(below, next);
    }
    int sources = 0;
    for (int d = 0; d < 4; ++d) sources += read(waterSide(p, d)) == WATER;
    // Pouring cells spread sideways only with three source neighbors. When
    // downward replacement is blocked, sources may still feed their banks.
    if (down ? sources < 3 : (current != WATER && waterHole(read, p))) return;
    if (waterAmount(current) <= 1) return;
    auto directions = waterSpreadDirections(read, p);
    for (int d = 0; d < 4; ++d) if (directions[d]) {
        auto q = waterSide(p, d);
        char next = computeWaterStateAt(read, q, sourceConversion);
        char old = read(q);
        // Horizontal spreading only replaces weaker fluid. Decay belongs to
        // that cell's own scheduled tick, preventing stale neighbors reviving it.
        if (isWater(next) && (next == WATER || !isWater(old) ||
            waterAmount(next) > waterAmount(old)) && next != old) write(q, next);
    }
}
