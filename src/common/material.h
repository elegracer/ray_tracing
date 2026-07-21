#pragma once

#include "common/openpbr_core.h"
#include "traits.h"

#include "common.h"
#include "ray.h"
#include "texture.h"
#include "pdf.h"

#include <stdexcept>
#include <vector>

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

    Vec3d evaluate_direct(const Ray&, const HitRecord&, const Vec3d&, double& pdf) const {
        pdf = 0.0;
        return Vec3d::Zero();
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

    Vec3d evaluate_direct(const Ray&, const HitRecord& hit_rec, const Vec3d& direction,
        double& pdf) const {
        const double cosine = std::max(0.0, hit_rec.normal.dot(direction.normalized()));
        pdf = cosine / pi;
        return m_tex->value(hit_rec.u, hit_rec.v, hit_rec.p) * pdf;
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
        scatter_rec.skip_pdf_ray = Ray(hit_rec.p, reflected, ray_in.time(),
            ray_in.subsurface_medium(), ray_in.subsurface_owner());

        return true;
    }

    double scattering_pdf(const Ray& ray_in, const HitRecord& hit_rec, const Ray& scattered) const {
        return 0.0;
    }

    Vec3d evaluate_direct(const Ray&, const HitRecord&, const Vec3d&, double& pdf) const {
        pdf = 0.0;
        return Vec3d::Zero();
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

        scatter_rec.skip_pdf_ray = Ray(hit_rec.p, direction, ray_in.time(),
            ray_in.subsurface_medium(), ray_in.subsurface_owner());
        return true;
    }

    double scattering_pdf(const Ray& ray_in, const HitRecord& hit_rec, const Ray& scattered) const {
        return 0.0;
    }

    Vec3d evaluate_direct(const Ray&, const HitRecord&, const Vec3d&, double& pdf) const {
        pdf = 0.0;
        return Vec3d::Zero();
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

struct OpenPbrSurfaceMaterial {
    OpenPbrSurfaceMaterial(const rt::OpenPbrCompiledMaterial& material,
        const std::vector<pro::proxy<Texture>>& textures)
        : material(material),
          base_color_texture(resolve_texture(material.color_textures.base_color, textures)),
          specular_color_texture(resolve_texture(material.color_textures.specular_color, textures)),
          transmission_color_texture(
              resolve_texture(material.color_textures.transmission_color, textures)),
          emission_color_texture(resolve_texture(material.color_textures.emission_color, textures)),
          base_metalness_texture(
              resolve_texture(material.scalar_textures.base_metalness, textures)),
          specular_roughness_texture(
              resolve_texture(material.scalar_textures.specular_roughness, textures)) {}

    Vec3d emitted(const Ray&, const HitRecord&, const double u, const double v,
        const Vec3d& p) const {
        const rt::OpenPbrCoreMaterial parameters = evaluated_emission_parameters(u, v, p);
        const rt::OpenPbrVec3 value = rt::emission_openpbr_core(parameters);
        return {value.x, value.y, value.z};
    }

    bool scatter(const Ray& ray_in, const HitRecord& hit_rec, ScatterRecord& scatter_rec) const {
        if (ray_in.subsurface_medium().active != 0 && ray_in.subsurface_owner() != this) {
            return false;
        }
        const rt::OpenPbrCoreMaterial parameters =
            evaluated_scattering_parameters(hit_rec.u, hit_rec.v, hit_rec.p);
        const Vec3d outward_normal = hit_rec.front_face ? hit_rec.normal : -hit_rec.normal;
        const rt::OpenPbrFrame frame =
            rt::make_openpbr_frame(to_openpbr(outward_normal), rt::OpenPbrVec3 {});
        const rt::OpenPbrSample sample = rt::sample_openpbr_core(parameters, frame,
            to_openpbr(-ray_in.direction().normalized()), static_cast<float>(random_double()),
            static_cast<float>(random_double()), static_cast<float>(random_double()));
        if (sample.valid == 0) {
            return false;
        }

        scatter_rec.attenuation = {sample.weight.x, sample.weight.y, sample.weight.z};
        scatter_rec.pdf = nullptr;
        scatter_rec.skip_pdf = true;
        rt::OpenPbrSubsurfaceMedium medium = ray_in.subsurface_medium();
        const void* medium_owner = ray_in.subsurface_owner();
        if (sample.event == rt::OpenPbrScatterEvent::subsurface_entry) {
            medium = rt::openpbr_subsurface_medium(parameters);
            medium_owner = this;
        } else if (sample.event == rt::OpenPbrScatterEvent::subsurface_exit) {
            medium = {};
            medium_owner = nullptr;
        }
        scatter_rec.skip_pdf_ray = Ray(hit_rec.p, Vec3d {sample.wi.x, sample.wi.y, sample.wi.z},
            ray_in.time(), medium, medium_owner);
        return true;
    }

    double scattering_pdf(const Ray& ray_in, const HitRecord& hit_rec, const Ray& scattered) const {
        const rt::OpenPbrCoreMaterial parameters =
            evaluated_scattering_parameters(hit_rec.u, hit_rec.v, hit_rec.p);
        const Vec3d outward_normal = hit_rec.front_face ? hit_rec.normal : -hit_rec.normal;
        const rt::OpenPbrFrame frame =
            rt::make_openpbr_frame(to_openpbr(outward_normal), rt::OpenPbrVec3 {});
        return static_cast<double>(
            rt::pdf_openpbr_core(parameters, frame, to_openpbr(-ray_in.direction().normalized()),
                to_openpbr(scattered.direction().normalized())));
    }

    Vec3d evaluate_direct(const Ray& ray_in, const HitRecord& hit_rec, const Vec3d& direction,
        double& pdf) const {
        const rt::OpenPbrCoreMaterial parameters =
            evaluated_scattering_parameters(hit_rec.u, hit_rec.v, hit_rec.p);
        const Vec3d outward_normal = hit_rec.front_face ? hit_rec.normal : -hit_rec.normal;
        const rt::OpenPbrFrame frame =
            rt::make_openpbr_frame(to_openpbr(outward_normal), rt::OpenPbrVec3 {});
        const rt::OpenPbrEvaluation evaluation = rt::evaluate_openpbr_core(parameters, frame,
            to_openpbr(-ray_in.direction().normalized()), to_openpbr(direction.normalized()));
        pdf = static_cast<double>(evaluation.pdf);
        const double cosine = std::abs(hit_rec.normal.dot(direction.normalized()));
        return Vec3d {evaluation.value.x, evaluation.value.y, evaluation.value.z} * cosine;
    }

private:
    template<typename Binding>
    static pro::proxy<Texture> resolve_texture(const Binding& binding,
        const std::vector<pro::proxy<Texture>>& textures) {
        if (binding.texture_index < 0) {
            return nullptr;
        }
        const std::size_t index = static_cast<std::size_t>(binding.texture_index);
        if (index >= textures.size()) {
            throw std::invalid_argument("OpenPBR texture binding index is out of range");
        }
        return textures[index];
    }

    static rt::OpenPbrVec3 sample_texture(const pro::proxy<Texture>& texture, double u, double v,
        const Vec3d& p) {
        const Vec3d value = texture->value(u, v, p);
        return {static_cast<float>(value.x()), static_cast<float>(value.y()),
            static_cast<float>(value.z())};
    }

    static void apply_binding(rt::OpenPbrCoreMaterial& parameters,
        const rt::OpenPbrColorTextureBinding& binding, rt::OpenPbrColorInput input,
        const pro::proxy<Texture>& texture, double u, double v, const Vec3d& p) {
        if (binding.texture_index < 0) {
            return;
        }
        rt::openpbr_apply_color_input(parameters, input, sample_texture(texture, u, v, p),
            binding.source_color_space);
    }

    static void apply_binding(rt::OpenPbrCoreMaterial& parameters,
        const rt::OpenPbrScalarTextureBinding& binding, rt::OpenPbrScalarInput input,
        const pro::proxy<Texture>& texture, double u, double v, const Vec3d& p) {
        if (binding.texture_index < 0) {
            return;
        }
        rt::openpbr_apply_scalar_input(parameters, input,
            static_cast<float>(texture->value(u, v, p).x()));
    }

    rt::OpenPbrCoreMaterial evaluated_scattering_parameters(double u, double v,
        const Vec3d& p) const {
        rt::OpenPbrCoreMaterial parameters = material.parameters;
        apply_binding(parameters, material.color_textures.base_color,
            rt::OpenPbrColorInput::base_color, base_color_texture, u, v, p);
        apply_binding(parameters, material.color_textures.specular_color,
            rt::OpenPbrColorInput::specular_color, specular_color_texture, u, v, p);
        apply_binding(parameters, material.color_textures.transmission_color,
            rt::OpenPbrColorInput::transmission_color, transmission_color_texture, u, v, p);
        apply_binding(parameters, material.scalar_textures.base_metalness,
            rt::OpenPbrScalarInput::base_metalness, base_metalness_texture, u, v, p);
        apply_binding(parameters, material.scalar_textures.specular_roughness,
            rt::OpenPbrScalarInput::specular_roughness, specular_roughness_texture, u, v, p);
        return parameters;
    }

    rt::OpenPbrCoreMaterial evaluated_emission_parameters(double u, double v,
        const Vec3d& p) const {
        rt::OpenPbrCoreMaterial parameters = material.parameters;
        apply_binding(parameters, material.color_textures.emission_color,
            rt::OpenPbrColorInput::emission_color, emission_color_texture, u, v, p);
        return parameters;
    }

    static rt::OpenPbrVec3 to_openpbr(const Vec3d& value) {
        return {static_cast<float>(value.x()), static_cast<float>(value.y()),
            static_cast<float>(value.z())};
    }

    rt::OpenPbrCompiledMaterial material;
    pro::proxy<Texture> base_color_texture;
    pro::proxy<Texture> specular_color_texture;
    pro::proxy<Texture> transmission_color_texture;
    pro::proxy<Texture> emission_color_texture;
    pro::proxy<Texture> base_metalness_texture;
    pro::proxy<Texture> specular_roughness_texture;
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

    Vec3d evaluate_direct(const Ray&, const HitRecord&, const Vec3d&, double& pdf) const {
        pdf = 0.0;
        return Vec3d::Zero();
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

    Vec3d evaluate_direct(const Ray&, const HitRecord& hit_rec, const Vec3d&, double& pdf) const {
        pdf = 1.0 / (4.0 * pi);
        return m_tex->value(hit_rec.u, hit_rec.v, hit_rec.p) * pdf;
    }

    pro::proxy<Texture> m_tex;
};
