#pragma once

#include "common/perlin.h"
#include "common/rtw_image.h"
#include "proxy/proxy.h"

#include "common.h"

PRO_DEF_MEM_DISPATCH(MemValue, value);

struct Texture                                                                                //
    : pro::facade_builder                                                                     //
      ::support_copy<pro::constraint_level::nontrivial>                                       //
      ::add_convention<MemValue, Vec3d(const double u, const double v, const Vec3d& p) const> //
      ::build {};

struct SolidColor {
    SolidColor(const Vec3d& albedo) : m_albedo(albedo) {}
    SolidColor(const double red, const double green, const double blue)
        : SolidColor(Vec3d {red, green, blue}) {}

    Vec3d value(const double u, const double v, const Vec3d& p) const { return m_albedo; }

    Vec3d m_albedo;
};

struct CheckerTexture {
    CheckerTexture(const double scale, const pro::proxy<Texture>& even,
        const pro::proxy<Texture>& odd)
        : m_inv_scale(1.0 / scale),
          m_even(even),
          m_odd(odd) {}

    CheckerTexture(const double scale, const Vec3d& color1, const Vec3d& color2)
        : CheckerTexture(scale, pro::make_proxy_shared<Texture, SolidColor>(color1),
              pro::make_proxy_shared<Texture, SolidColor>(color2)) {}

    Vec3d value(const double u, const double v, const Vec3d& p) const {
        const int x = std::floor(m_inv_scale * p.x());
        const int y = std::floor(m_inv_scale * p.y());
        const int z = std::floor(m_inv_scale * p.z());

        const bool isEven = (x + y + z) % 2 == 0;

        return isEven ? m_even->value(u, v, p) : m_odd->value(u, v, p);
    }

    double m_inv_scale;
    pro::proxy<Texture> m_even;
    pro::proxy<Texture> m_odd;
};

struct ImageTexture {
    ImageTexture(const std::string& img_filepath) : m_image(img_filepath) {}

    Vec3d value(const double u, const double v, const Vec3d& p) const {
        // If we have no texture data, then return solid cyan as a debugging aid.
        if (m_image.height() <= 0) {
            return {0.0, 1.0, 1.0};
        }

        // Clamp input texture coordinates to [0,1] x [1,0]
        const double clamped_u = std::clamp(u, 0.0, 1.0);
        // Flip Y to image coordinates
        const double clamped_v = std::clamp(1.0 - v, 0.0, 1.0);

        const int i = int(clamped_u * m_image.width());
        const int j = int(clamped_v * m_image.height());

        return m_image.pixel_data(i, j);
    }

    RTWImage m_image;
};

struct NoiseTexture {
    NoiseTexture(const double scale) : m_scale(scale) {}

    Vec3d value(const double u, const double v, const Vec3d& p) const {
        return Vec3d {1.0, 1.0, 1.0} * m_noise.turb(p, 7);
    }

    Perlin m_noise;
    double m_scale;
};
