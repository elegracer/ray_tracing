#pragma once

#include "proxy/proxy.h"

#include "common.h"
#include "ray.h"

struct HitRecord;

PRO_DEF_MEM_DISPATCH(MemScatter, scatter);

struct Material
    : pro::facade_builder ::support_copy<pro::constraint_level::nontrivial>::add_convention<
          MemScatter, bool(const Ray& ray_in, const HitRecord& hit_rec, Vec3d& attenuation,
                          Ray& scattered) const>::build {};

struct HitRecord {
    Vec3d p;
    Vec3d normal;
    pro::proxy<Material> mat;
    double t;
    bool front_face;

    void set_face_normal(const Ray& ray, const Vec3d& outward_normal) {
        // Sets the hit record normal vector
        // NOTE: the parameter `outward_normal` is assumed to have unit length
        front_face = ray.direction().dot(outward_normal) < 0.0;
        normal = front_face ? outward_normal : -outward_normal;
    }
};


struct Lambertion {
    Lambertion(const Vec3d& albedo) : albedo(albedo) {}

    bool scatter(const Ray& ray_in, const HitRecord& hit_rec, Vec3d& attenuation,
        Ray& scattered) const {
        Vec3d scatter_direction = hit_rec.normal + random_unit_vector();
        if (scatter_direction.isZero(1e-8)) {
            scatter_direction = hit_rec.normal;
        }

        scattered = Ray(hit_rec.p, scatter_direction);
        attenuation = albedo;
        return true;
    }

private:
    Vec3d albedo;
};


struct Metal {
    Metal(const Vec3d& albedo, const double fuzz) : albedo(albedo), fuzz(fuzz < 1.0 ? fuzz : 1.0) {}

    bool scatter(const Ray& ray_in, const HitRecord& hit_rec, Vec3d& attenuation,
        Ray& scattered) const {
        Vec3d reflected = reflect(ray_in.direction(), hit_rec.normal);
        reflected = reflected.normalized() + (fuzz * random_unit_vector());
        scattered = Ray(hit_rec.p, reflected);
        attenuation = albedo;
        return scattered.direction().dot(hit_rec.normal) > 0.0;
    }

private:
    Vec3d albedo;
    double fuzz;
};


struct Dielectric {
    Dielectric(const double refraction_index) : refraction_index(refraction_index) {}

    bool scatter(const Ray& ray_in, const HitRecord& hit_rec, Vec3d& attenuation,
        Ray& scattered) const {
        attenuation = {1.0, 1.0, 1.0};
        const double ri = hit_rec.front_face ? (1.0 / refraction_index) : refraction_index;

        const Vec3d unit_direction = ray_in.direction().normalized();
        const Vec3d refracted = refract(unit_direction, hit_rec.normal, ri);

        scattered = Ray(hit_rec.p, refracted);
        return true;
    }

private:
    // Refractive index in vaccum or air, or the ratio of the material's refractive index onver the
    // refractive index of the enclosing media
    double refraction_index;
};
