#pragma once

#include "common.h"
#include "aabb.h"
#include "material.h"

class Sphere {
public:
    Sphere() = delete;

    // Stationary Sphere
    Sphere(const Vec3d& static_center, const double radius, pro::proxy<Material> mat)
        : m_center(static_center, {0.0, 0.0, 0.0}),
          m_radius(std::max(0.0, radius)),
          m_mat(mat) {
        const Vec3d radius_vec {radius, radius, radius};
        m_bbox = AABB {static_center - radius_vec, static_center + radius_vec};
    }

    // Moving Sphere
    Sphere(const Vec3d& center1, const Vec3d& center2, const double radius,
        pro::proxy<Material> mat)
        : m_center(center1, center2 - center1),
          m_radius(std::max(0.0, radius)),
          m_mat(mat) {
        const Vec3d radius_vec {radius, radius, radius};
        m_bbox = AABB {
            AABB {m_center.at(0.0) - radius_vec, m_center.at(0.0) + radius_vec}, //
            AABB {m_center.at(1.0) - radius_vec, m_center.at(1.0) + radius_vec}  //
        };
    }


    AABB bounding_box() const { return m_bbox; }

    double pdf_value(const Vec3d& origin, const Vec3d& direction) const {
        // This method only works for stationary spheres

        HitRecord hit_rec;
        if (!hit(Ray {origin, direction}, Interval {0.001, infinity}, hit_rec)) {
            return 0.0;
        }

        const double distance_sq = (m_center.at(0.0) - origin).squaredNorm();
        const double cos_theta_max = std::sqrt(1.0 - m_radius * m_radius / distance_sq);
        const double solid_angle = 2.0 * pi * (1.0 - cos_theta_max);

        return std::max(1e-8, 1.0 / solid_angle);
    }

    Vec3d random(const Vec3d& origin) const {
        const Vec3d direction = m_center.at(0.0) - origin;
        const double distance_sq = direction.squaredNorm();
        const ONB uvw {direction};
        return uvw.from_basis(random_to_sphere(m_radius, distance_sq));
    }

    bool hit(const Ray& ray, const Interval& ray_t, HitRecord& hit_rec) const {
        const Vec3d current_center = m_center.at(ray.time());
        const Vec3d oc = current_center - ray.origin();
        const double a = ray.direction().squaredNorm();
        const double h = ray.direction().dot(oc);
        const double c = oc.squaredNorm() - m_radius * m_radius;
        const double discriminant = h * h - a * c;

        if (discriminant < 0.0) {
            return false;
        }

        const double sqrtd = std::sqrt(discriminant);

        // Find the nearest root that lies in the acceptable range
        double root = (h - sqrtd) / a;
        if (!ray_t.surrounds(root)) {
            root = (h + sqrtd) / a;
            if (!ray_t.surrounds(root)) {
                return false;
            }
        }

        hit_rec.t = root;
        hit_rec.p = ray.at(hit_rec.t);
        const Vec3d outward_normal = (hit_rec.p - current_center) / m_radius;
        hit_rec.set_face_normal(ray, outward_normal);
        get_sphere_uv(outward_normal, hit_rec.u, hit_rec.v);
        hit_rec.mat = m_mat;

        return true;
    }

    static void get_sphere_uv(const Vec3d& p, double& u, double& v) {
        // p: a given point on the sphere of radius one, centered at the origin.
        // u: returned value [0,1] of angle around the Y axis from X=-1.
        // v: returned value [0,1] of angle from Y=-1 to Y=+1.
        //     <1 0 0> yields <0.50 0.50>      <-1  0  0> yields <0.00 0.50>
        //     <0 1 0> yields <0.50 1.00>      < 0 -1  0> yields <0.50 0.00>
        //     <0 0 1> yields <0.25 0.50>      < 0  0 -1> yields <0.75 0.50>

        const double theta = std::acos(-p.y());
        const double phi = std::atan2(-p.z(), p.x()) + pi;

        u = phi / (2.0 * pi);
        v = theta / pi;
    }

    static Vec3d random_to_sphere(const double radius, const double distance_sq) {
        const double r1 = random_double();
        const double r2 = random_double();

        const double z = 1.0 + r2 * (std::sqrt(1.0 - radius * radius / distance_sq) - 1.0);

        const double phi = 2.0 * pi * r1;
        const double x = std::cos(phi) * std::sqrt(1.0 - z * z);
        const double y = std::sin(phi) * std::sqrt(1.0 - z * z);

        return {x, y, z};
    }

private:
    Ray m_center;
    double m_radius;
    pro::proxy<Material> m_mat;
    AABB m_bbox;
};
