
#pragma once

#include "ft_vox.hpp"
#include "NoiseGenerator.hpp"
#include "SubChunk.hpp"
#include "CaveGenerator.hpp"
#include "Chunk.hpp"
#include "Camera.hpp"
#include "Chrono.hpp"
#include "ThreadPool.hpp"

#include <unordered_set>
#include <queue>
#include <list>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <limits>

struct FrustumPlane { glm::vec3 n; float d; };
struct Frustum {
	FrustumPlane p[6]; // L,R,B,T,N,F
	Frustum() {
		for (auto &pl : p) { pl.n = glm::vec3(0.0f); pl.d = 0.0f; }
	}
	static Frustum fromVP(const glm::mat4& VP) {
		Frustum f{}; const glm::mat4& m = VP; auto norm = [](FrustumPlane& pl){
			float inv = 1.0f / glm::length(pl.n); pl.n *= inv; pl.d *= inv;
		};
		auto row = [&](int i){ return glm::vec4(m[0][i], m[1][i], m[2][i], m[3][i]); };
		glm::vec4 r0=row(0), r1=row(1), r2=row(2), r3=row(3);

		auto set = [&](FrustumPlane& pl, const glm::vec4& v){
			pl.n = glm::vec3(v.x, v.y, v.z); pl.d = v.w; norm(pl);
		};
		set(f.p[0], r3 + r0); // Left
		set(f.p[1], r3 - r0); // Right
		set(f.p[2], r3 + r1); // Bottom
		set(f.p[3], r3 - r1); // Top
		set(f.p[4], r3 + r2); // Near
		set(f.p[5], r3 - r2); // Far
		return f;
	}
	bool aabbVisible(const glm::vec3& mn, const glm::vec3& mx) const {
		glm::vec3 c = (mn + mx) * 0.5f;
		glm::vec3 e = (mx - mn) * 0.5f;
		for (int i=0;i<6;++i) {
			const auto& pl = p[i];
			float r = e.x * std::abs(pl.n.x) + e.y * std::abs(pl.n.y) + e.z * std::abs(pl.n.z);
			float s = glm::dot(pl.n, c) + pl.d;
			if (s + r < 0.0f) return false;
		}
		return true;
	}
};
