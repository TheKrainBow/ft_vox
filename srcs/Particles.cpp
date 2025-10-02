#include "Particles.hpp"

static GLuint compileFileShader(const char* vsPath, const char* fsPath)
{
    return createShaderProgram(vsPath, fsPath);
}

ParticleSystem::~ParticleSystem()
{
    if (_quadVBO) glDeleteBuffers(1, &_quadVBO);
    if (_instanceVBO) glDeleteBuffers(1, &_instanceVBO);
    if (_vao) glDeleteVertexArrays(1, &_vao);
    if (_program) glDeleteProgram(_program);
}

void ParticleSystem::initGL()
{
    // Shader
    _program = compileFileShader("shaders/render/particle.vert", "shaders/render/particle.frag");

    // Base quad (centered at origin, unit size). We'll scale in shader per instance.
    const float quad[8] = {
        -0.5f, -0.5f,
         0.5f, -0.5f,
        -0.5f,  0.5f,
         0.5f,  0.5f,
    };

    glGenVertexArrays(1, &_vao);
    glBindVertexArray(_vao);

    glGenBuffers(1, &_quadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, _quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glGenBuffers(1, &_instanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, _instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    // Layout: vec3 pos, float size, vec3 color, float alpha(life), int kind
    GLsizei stride = (GLsizei)(sizeof(float) * (3 + 1 + 3 + 1) + sizeof(int));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glVertexAttribDivisor(1, 1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glVertexAttribDivisor(2, 1);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(4 * sizeof(float)));
    glVertexAttribDivisor(3, 1);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, (void*)(7 * sizeof(float)));
    glVertexAttribDivisor(4, 1);

    glEnableVertexAttribArray(5);
    glVertexAttribIPointer(5, 1, GL_INT, stride, (void*)(8 * sizeof(float)));
    glVertexAttribDivisor(5, 1);

    glBindVertexArray(0);
}

void ParticleSystem::spawnBurst(const glm::vec3& center, const glm::vec3& color, int count)
{
    static std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> r01(0.0f, 1.0f);
    std::uniform_real_distribution<float> jitter(-0.5f, 0.5f);

    _particles.reserve(_particles.size() + count);
    for (int i = 0; i < count; ++i)
    {
        Particle p;
        // Spawn within the block volume
        p.pos = center + glm::vec3(jitter(rng), jitter(rng), jitter(rng));
        // Random velocity biased outward
        glm::vec3 v = glm::normalize(glm::vec3(jitter(rng), r01(rng) * 1.2f, jitter(rng)) + 0.01f);
        float spd = 2.0f * r01(rng) + 1.0f;
        p.vel = v * spd;
        float lum = 0.9f + 0.2f * r01(rng); // small brightness jitter
        p.color = glm::clamp(color * lum, 0.0f, 1.0f);
        p.life = 0.8f + 0.6f * r01(rng);
        p.size = 0.05f + 0.05f * r01(rng);
        _particles.push_back(p);
    }
    _dirtyGPU = true;
}

void ParticleSystem::spawnBurstColored(const glm::vec3& center, const std::vector<glm::vec3>& colors, int kind)
{
    if (colors.empty()) return;
    static std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> r01(0.0f, 1.0f);
    std::uniform_real_distribution<float> jitter(-0.5f, 0.5f);

    _particles.reserve(_particles.size() + colors.size());
    for (size_t i = 0; i < colors.size(); ++i)
    {
        const glm::vec3& color = colors[i % colors.size()];
        Particle p;
        p.pos = center + glm::vec3(jitter(rng), jitter(rng), jitter(rng));
        glm::vec3 v = glm::normalize(glm::vec3(jitter(rng), r01(rng) * 1.2f, jitter(rng)) + 0.01f);
        float spd = 2.0f * r01(rng) + 1.0f;
        p.vel = v * spd;
        float lum = 0.9f + 0.2f * r01(rng);
        p.color = glm::clamp(color * lum, 0.0f, 1.0f);
        p.life = 0.8f + 0.6f * r01(rng);
        p.size = 0.05f + 0.05f * r01(rng);
        p.kind = kind;
        _particles.push_back(p);
    }
    _dirtyGPU = true;
}

void ParticleSystem::update(float dt, const std::function<bool(const glm::vec3&)>& isSolid)
{
    if (_particles.empty()) return;
    const glm::vec3 gravity(0.0f, -9.8f, 0.0f);
    const float drag = 0.90f;

    // Compact in-place
    size_t write = 0;
    for (size_t i = 0; i < _particles.size(); ++i)
    {
        Particle p = _particles[i];
        p.vel += gravity * dt * 0.8f;
        p.vel *= std::pow(drag, dt * 60.0f);
        glm::vec3 newPos = p.pos + p.vel * dt;
        bool collided = false;
        if (isSolid)
        {
            // Ground-only collision: test cell just below target position
            glm::vec3 probe = newPos;
            probe.y -= 0.05f; // small radius
            if (p.vel.y < 0.0f && isSolid(probe))
            {
                // Place on top of the voxel and bounce
                float groundY = std::floor(newPos.y) + 1.001f;
                newPos.y = std::max(newPos.y, groundY);
                p.vel.y *= -0.20f;        // small bounce
                p.vel.x *= 0.6f;          // friction
                p.vel.z *= 0.6f;
                collided = true;
                // Fade after settling
                if (std::abs(p.vel.y) < 0.05f)
                    p.life = std::min(p.life, 0.35f);
            }
        }
        p.pos = newPos;
        (void)collided;
        p.life -= dt;
        if (p.life > 0.0f)
        {
            _particles[write++] = p;
        }
    }
    if (write != _particles.size())
    {
        _particles.resize(write);
    }
    _dirtyGPU = true;
}

void ParticleSystem::uploadInstances()
{
    if (!_dirtyGPU) return;
    _dirtyGPU = false;
    glBindBuffer(GL_ARRAY_BUFFER, _instanceVBO);
    if (_particles.empty())
    {
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
        return;
    }
    struct Inst { float x,y,z, size, r,g,b, a; int kind; };
    std::vector<Inst> tmp;
    tmp.reserve(_particles.size());
    for (const Particle& p : _particles)
    {
        float alpha = glm::clamp(p.life, 0.0f, 1.0f);
        tmp.push_back({p.pos.x, p.pos.y, p.pos.z, p.size, p.color.r, p.color.g, p.color.b, alpha, p.kind});
    }
    glBufferData(GL_ARRAY_BUFFER, tmp.size() * sizeof(Inst), tmp.data(), GL_DYNAMIC_DRAW);
}

void ParticleSystem::render(const glm::mat4& viewRot,
                            const glm::mat4& proj,
                            const glm::vec3& cameraPos,
                            int timeValue,
                            const glm::vec3& lightColor)
{
    if (_particles.empty()) return;
    uploadInstances();

    glUseProgram(_program);
    glUniformMatrix4fv(glGetUniformLocation(_program, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(_program, "viewRot"), 1, GL_FALSE, glm::value_ptr(viewRot));
    glUniform3fv(glGetUniformLocation(_program, "cameraPos"), 1, glm::value_ptr(cameraPos));
    glUniform1i(glGetUniformLocation(_program, "timeValue"), timeValue);
    glUniform3fv(glGetUniformLocation(_program, "lightColor"), 1, glm::value_ptr(lightColor));

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glBindVertexArray(_vao);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)_particles.size());
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}
