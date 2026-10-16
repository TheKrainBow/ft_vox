#include "ChunkRenderer.hpp"
#include <cassert>
#include <cmath>
#include <iostream>

bool isWSL() { return false; }

int main() {
    std::mutex mutex;
    std::queue<DisplayData*> solids,transparent;
    ChunkRenderer renderer(mutex,mutex,solids,transparent);
    const glm::vec3 eye(16,16,16);
    assert(renderer.furthestLoadedDistance(eye)==0);
    auto stage=[&](std::initializer_list<glm::vec4> solidOrigins,
                   std::initializer_list<glm::vec4> transparentOrigins) {
        auto* solid=new DisplayData;
        auto* liquid=new DisplayData;
        solid->ssboData=solidOrigins;
        liquid->ssboData=transparentOrigins;
        solids.push(solid);
        transparent.push(liquid);
        return solid;
    };
    auto close=[](float actual,float expected) { assert(std::abs(actual-expected)<0.001f); };
    stage({{0,0,0,1}},{});
    assert(renderer.furthestLoadedDistance(eye)==0); // Queued work is not yet displayed.
    renderer.updateDrawData();
    const float nearby=std::sqrt(768.0f); // Farthest corner of the cube around the eye.
    close(renderer.furthestLoadedDistance(eye),nearby);
    stage({{0,0,0,1}},{{0,320,0,1}});
    close(renderer.furthestLoadedDistance(eye),nearby);
    renderer.updateDrawData();
    const float tall=std::sqrt(113408.0f); // Vertical extent matters, including liquid-only layers.
    close(renderer.furthestLoadedDistance(eye),tall);
    assert(renderer.furthestLoadedDistance({16,160,16})<tall); // Moving toward the loaded volume.
    assert(renderer.furthestLoadedDistance({16,656,16})>tall); // Flying above it.
    stage({{0,0,0,1}},{});
    renderer.updateDrawData();
    close(renderer.furthestLoadedDistance(eye),nearby); // Unloading must shrink fog again.
    auto* data=stage({{-96,-128,-32,1}},{});
    renderer.updateDrawData();
    data->vertexData.clear();
    data->indirectBufferData.clear();
    close(renderer.furthestLoadedDistance({0,0,0}),std::sqrt(26624.0f));
    stage({},{});
    renderer.updateDrawData();
    assert(renderer.furthestLoadedDistance(eye)==0);
    std::cout << "Fog distance follows active solid/liquid meshes, 3D movement and unloading\n";
}
