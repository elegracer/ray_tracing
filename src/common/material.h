#pragma once

#include "traits.h"

#include "common.h"
#include "ray.h"
#include "texture.h"
#include "pdf.h"

struct HitRecord {
    Vec3d p;
    Vec3d normal;
    pro::proxy<Material> mat;
    double t;
    double u;
    double v;
    bool front_face;

    void set_face_normal(const Ray& ray, const Vec3d& outward_normal) {
        // Sets the hit record normal vector
        // NOTE: the parameter `outward_normal` is assumed to have unit length
        front_face = ray.direction().dot(outward_normal) < 0.0;
        normal = front_face ? outward_normal : -outward_normal;
    }
};

struct ScatterRecord {
    Vec3d attenuation;
    pro::proxy<PDF> pdf;
    bool skip_pdf;
    Ray skip_pdf_ray;
};


struct EmptyMaterial {

    Vec3d emitted(const Ray& ray_in, const HitRecord& hit_rec, const double u, const double v,
        const Vec3d& p) const {
        return {0.0, 0.0, 0.0};
    }

    bool scatter(const Ray& ray_in, const HitRecord& hit_rec, ScatterRecord& scatter_rec) const {
        return false;
    }

    double scattering_pdf(const Ray& ray_in, const HitRecord& hit_rec, const Ray& scattered) const {
        return 0.0;
    }
};


struct Lambertion {
    explicit Lambertion(const Vec3d& albedo)
        : m_tex(pro::make_proxy_shared<Texture, SolidColor>(albedo)) {}
    explicit Lambertion(const pro::proxy<Texture>& tex) : m_tex(tex) {}

    Vec3d emitted(const Ray& ray_in, const HitRecord& hit_rec, const double u, const double v,
        const Vec3d& p) const {
        return {0.0, 0.0, 0.0};
    }

    bool scatter(const Ray& ray_in, const HitRecord& hit_rec, ScatterRecord& scatter_rec) const {
        scatter_rec.attenuation = m_tex->value(hit_rec.u, hit_rec.v, hit_rec.p);
        scatter_rec.pdf = pro::make_proxy_shared<PDF, CosinePDF>(hit_rec.normal);
        scatter_rec.skip_pdf = false;
        return true;
    }

    double scattering_pdf(const Ray& ray_in, const HitRecord& hit_rec, const Ray& scattered) const {
        const double cos_theta = hit_rec.normal.dot(scattered.direction().normalized());
        return std::max(1e-8, cos_theta / pi);
    }

private:
    pro::proxy<Texture> m_tex;
};


struct Metal {
    Metal(const Vec3d& albedo, const double fuzz) : albedo(albedo), fuzz(fuzz < 1.0 ? fuzz : 1.0) {}

    Vec3d emitted(const Ray& ray_in, const HitRecord& hit_rec, const double u, const double v,
        const Vec3d& p) const {
        return {0.0, 0.0, 0.0};
    }

    bool scatter(const Ray& ray_in, const HitRecord& hit_rec, ScatterRecord& scatter_rec) const {
        const Vec3d reflected = reflect(ray_in.direction(), hit_rec.normal).normalized()
                                + (fuzz * random_unit_vector());

        scatter_rec.attenuation = albedo;
        scatter_rec.pdf = nullptr;
        scatter_rec.skip_pdf = true;
        scatter_rec.skip_pdf_ray = Ray(hit_rec.p, reflected, ray_in.time());

        return true;
    }

    double scattering_pdf(const Ray& ray_in, const HitRecord& hit_rec, const Ray& scattered) const {
        return 0.0;
    }

private:
    Vec3d albedo;
    double fuzz;
};


struct Dielectric {
    Dielectric(const double refraction_index) : refraction_index(refraction_index) {}

    Vec3d emitted(const Ray& ray_in, const HitRecord& hit_rec, const double u, const double v,
        const Vec3d& p) const {
        return {0.0, 0.0, 0.0};
    }

    bool scatter(const Ray& ray_in, const HitRecord& hit_rec, ScatterRecord& scatter_rec) const {
        scatter_rec.attenuation = {1.0, 1.0, 1.0};
        scatter_rec.pdf = nullptr;
        scatter_rec.skip_pdf = true;

        const double ri = hit_rec.front_face ? (1.0 / refraction_index) : refraction_index;

        const Vec3d unit_direction = ray_in.direction().normalized();
        const double cos_theta = std::min(-unit_direction.dot(hit_rec.normal), 1.0);
        const double sin_theta = std::sqrt(1.0 - cos_theta * cos_theta);

        const bool cannot_refract = ri * sin_theta > 1.0;
        const Vec3d direction =
            (cannot_refract || reflectance(cos_theta, refraction_index) > random_double())
                ? reflect(unit_direction, hit_rec.normal)
                : refract(unit_direction, hit_rec.normal, ri);

        scatter_rec.skip_pdf_ray = Ray(hit_rec.p, direction, ray_in.time());
        return true;
    }

    double scattering_pdf(const Ray& ray_in, const HitRecord& hit_rec, const Ray& scattered) const {
        return 0.0;
    }

private:
    // Refractive index in vaccum or air, or the ratio of the material's refractive index onver the
    // refractive index of the enclosing media
    double refraction_index;

    static double reflectance(const double cosine, const double refraction_index) {
        // Use Schlick's approximation for reflectance.
        const double r0 = (1.0 - refraction_index) / (1.0 + refraction_index);
        const double r0sq = r0 * r0;
        return r0sq + (1.0 - r0sq) * std::pow(1.0 - cosine, 5);
    }
};

struct DiffuseLight {
    explicit DiffuseLight(const Vec3d& emit)
        : m_tex(pro::make_proxy_shared<Texture, SolidColor>(emit)) {}
    explicit DiffuseLight(const pro::proxy<Texture>& tex) : m_tex(tex) {}

    Vec3d emitted(const Ray& ray_in, const HitRecord& hit_rec, const double u, const double v,
        const Vec3d& p) const {
        if (!hit_rec.front_face) {
            return {0.0, 0.0, 0.0};
        }
        return m_tex->value(u, v, p);
    }

    bool scatter(const Ray& ray_in, const HitRecord& hit_rec, ScatterRecord& scatter_rec) const {
        return false;
    }

    double scattering_pdf(const Ray& ray_in, const HitRecord& hit_rec, const Ray& scattered) const {
        return 0.0;
    }

    pro::proxy<Texture> m_tex;
};

struct Isotropic {
    explicit Isotropic(const Vec3d& albedo)
        : m_tex(pro::make_proxy_shared<Texture, SolidColor>(albedo)) {}
    explicit Isotropic(const pro::proxy<Texture>& tex) : m_tex(tex) {}

    Vec3d emitted(const Ray& ray_in, const HitRecord& hit_rec, const double u, const double v,
        const Vec3d& p) const {
        return {0.0, 0.0, 0.0};
    }

    bool scatter(const Ray& ray_in, const HitRecord& hit_rec, ScatterRecord& scatter_rec) const {
        scatter_rec.attenuation = m_tex->value(hit_rec.u, hit_rec.v, hit_rec.p);
        scatter_rec.pdf = pro::make_proxy_shared<PDF, SpherePDF>();
        scatter_rec.skip_pdf = false;
        return true;
    }

    double scattering_pdf(const Ray& ray_in, const HitRecord& hit_rec, const Ray& scattered) const {
        return 1.0 / (4.0 * pi);
    }

    pro::proxy<Texture> m_tex;
};
