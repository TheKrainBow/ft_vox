#pragma once
#include "LavaLighting.hpp"
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

// World reads and field construction never run on the rendering thread.
// Keep one pending revision and one completed result instead of queuing builds.
class LavaLightWorker {
public:
    struct Result {
        uint64_t revision;
        std::shared_ptr<const LavaLightGrid> grid;
    };
    explicit LavaLightWorker(std::function<std::vector<LavaEmitter>()> collect)
        : _collect(std::move(collect)), _thread([this] { run(); }) {}
    ~LavaLightWorker() {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _stop = true;
        }
        _wake.notify_one();
        _thread.join();
    }
    void request(uint64_t revision) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_requested == revision) return;
            _requested = revision;
            _pending = true;
        }
        _wake.notify_one();
    }
    std::unique_ptr<Result> takeReady() {
        std::lock_guard<std::mutex> lock(_mutex);
        return std::move(_ready);
    }
private:
    void run() {
        std::vector<LavaEmitter> previous;
        std::shared_ptr<const LavaLightGrid> grid;
        for (;;) {
            uint64_t revision;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _wake.wait(lock, [this] { return _stop || _pending; });
                if (_stop) return;
                revision = _requested;
                _pending = false;
            }
            auto emitters = _collect();
            if (!grid || previous != emitters) {
                grid = std::make_shared<LavaLightGrid>(buildLavaLightGrid(emitters));
                previous = std::move(emitters);
            }
            auto ready = std::make_unique<Result>(Result{revision, grid});
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _ready.swap(ready);
            }
            // Free an unconsumed old field outside the rendering-side lock.
        }
    }
    std::function<std::vector<LavaEmitter>()> _collect;
    std::mutex _mutex;
    std::condition_variable _wake;
    bool _stop = false, _pending = false;
    uint64_t _requested = 0;
    std::unique_ptr<Result> _ready;
    std::thread _thread;
};
