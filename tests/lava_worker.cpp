#include "LavaLightWorker.hpp"
#include <cassert>
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>

static std::unique_ptr<LavaLightWorker::Result> waitReady(LavaLightWorker& worker) {
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    for (;;) {
        if (auto ready=worker.takeReady()) return ready;
        assert(std::chrono::steady_clock::now()<deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
int main() {
    std::promise<void> started, release, secondStarted, secondRelease;
    auto released=release.get_future();
    auto secondReleased=secondRelease.get_future();
    std::atomic_int calls{0};
    const auto mainThread=std::this_thread::get_id();
    LavaLightWorker worker([&] {
        assert(std::this_thread::get_id()!=mainThread);
        int call=++calls;
        if (call==1) { started.set_value(); released.wait(); }
        if (call==2) { secondStarted.set_value(); secondReleased.wait(); }
        return call<4 ? std::vector<LavaEmitter>{{0.5f,32.8667f,0.5f,0}} : std::vector<LavaEmitter>{};
    });
    worker.request(1);
    started.get_future().wait();
    // These rendering operations must return while world collection is blocked.
    // Only this thread can release the collector: blocking here fails the test.
    for (uint64_t revision=2;revision<=1000;++revision) {
        worker.request(revision);
        assert(!worker.takeReady());
    }
    assert(calls==1);
    release.set_value();
    secondStarted.get_future().wait();
    auto first=waitReady(worker);
    assert(first->revision==1 && first->grid->mask>0);
    secondRelease.set_value();
    auto latest=waitReady(worker);
    assert(latest->revision==1000 && calls==2);
    assert(latest->grid==first->grid); // No rebuild/upload for identical faces.
    worker.request(1001);
    auto reused=waitReady(worker);
    assert(reused->grid==latest->grid && reused->revision==1001 && calls==3);
    worker.request(1002);
    auto removed=waitReady(worker);
    assert(removed->grid!=reused->grid && removed->grid->mask==0 && calls==4);
    std::cout << "Background lighting: nonblocking polling, revision coalescing, field reuse and removal passed\n";
}
