#pragma once

#include "ft_vox.hpp"

struct Particle
{
    glm::vec3 pos;
    glm::vec3 vel;
    glm::vec3 color;
    float     life;   // seconds remaining
    float     size;   // in meters
    int       kind;   // 0=generic, 1=water
};

class ParticleSystem
{
public:
    ParticleSystem() = default;
    ~ParticleSystem();

    void initGL();

    // Update with optional collision callback: returns true if position is inside solid voxel
    void update(float dt, const std::function<bool(const glm::vec3&)>& isSolid = nullptr);
    void render(const glm::mat4& viewRot,
                const glm::mat4& proj,
                const glm::vec3& cameraPos,
                int timeValue,
                const glm::vec3& lightColor);

    // Spawn a small burst of particles within a 1m cube centered at `center`.
    void spawnBurst(const glm::vec3& center, const glm::vec3& color, int count = 24);
    void spawnBurstColored(const glm::vec3& center, const std::vector<glm::vec3>& colors, int kind = 0);
    void spawnWaterBurst(const glm::vec3& center, const std::vector<glm::vec3>& colors) { spawnBurstColored(center, colors, 1); }

private:
    std::vector<Particle> _particles;
    GLuint _vao = 0;
    GLuint _quadVBO = 0;
    GLuint _instanceVBO = 0;
    GLuint _program = 0;

    bool _dirtyGPU = true;

    void uploadInstances();
};
