#pragma once

#include "common/common.h"
#include <array>
#include <numeric>

struct Perlin {
    Perlin() {
        for (auto& elem : m_rand_float) {
            elem = random_double();
        }
        perlin_generate_perm(m_perm_x);
        perlin_generate_perm(m_perm_y);
        perlin_generate_perm(m_perm_z);
    }

    double noise(const Vec3d& p) const {
        const int i = int(4 * p.x()) & 255;
        const int j = int(4 * p.y()) & 255;
        const int k = int(4 * p.z()) & 255;
        return m_rand_float[m_perm_x[i] ^ m_perm_y[j] ^ m_perm_z[k]];
    }

private:
    static constexpr int s_point_count = 256;
    std::array<double, s_point_count> m_rand_float;
    std::array<int, s_point_count> m_perm_x;
    std::array<int, s_point_count> m_perm_y;
    std::array<int, s_point_count> m_perm_z;

    static void perlin_generate_perm(std::array<int, s_point_count>& p) {
        std::iota(p.begin(), p.end(), 0);

        for (int i = s_point_count - 1; i > 0; --i) {
            const int target_idx = random_int(0, i);
            std::swap(p[i], p[target_idx]);
        }
    }
};
