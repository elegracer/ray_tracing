#pragma once

#include "common/common.h"
#include <array>
#include <numeric>

struct Perlin {
    Perlin() {
        for (auto& elem : m_rand_vec) {
            elem = Vec3d::Random().normalized();
        }
        perlin_generate_perm(m_perm_x);
        perlin_generate_perm(m_perm_y);
        perlin_generate_perm(m_perm_z);
    }

    double noise(const Vec3d& p) const {
        const double u = p.x() - std::floor(p.x());
        const double v = p.y() - std::floor(p.y());
        const double w = p.z() - std::floor(p.z());

        const int i = int(std::floor(p.x()));
        const int j = int(std::floor(p.y()));
        const int k = int(std::floor(p.z()));

        Vec3d c[2][2][2];
        for (int di = 0; di < 2; ++di) {
            for (int dj = 0; dj < 2; ++dj) {
                for (int dk = 0; dk < 2; ++dk) {
                    c[di][dj][dk] = m_rand_vec[m_perm_x[(i + di) & 255] ^ m_perm_y[(j + dj) & 255]
                                               ^ m_perm_z[(k + dk) & 255]];
                }
            }
        }

        return trilinear_interp(c, u, v, w);
    }

private:
    static constexpr int s_point_count = 256;
    std::array<Vec3d, s_point_count> m_rand_vec;
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

    static double trilinear_interp(const Vec3d c[2][2][2], const double u, const double v,
        const double w) {
        const double uu = u * u * (3.0 - 2.0 * u);
        const double vv = v * v * (3.0 - 2.0 * v);
        const double ww = w * w * (3.0 - 2.0 * w);

        double accum = 0.0;
        for (int i = 0; i < 2; ++i) {
            for (int j = 0; j < 2; ++j) {
                for (int k = 0; k < 2; ++k) {
                    const Vec3d weight_vec {u - i, v - j, w - k};
                    accum += (i * uu + (1 - i) * (1.0 - uu)) * (j * vv + (1 - j) * (1.0 - vv))
                             * (k * ww + (1 - k) * (1.0 - ww)) * c[i][j][k].dot(weight_vec);
                }
            }
        }
        return accum;
    }
};
