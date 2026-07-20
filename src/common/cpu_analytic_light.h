#pragma once

#include "common/analytic_light.h"

#include <Eigen/Core>

#include <vector>

class Ray;
struct Interval;

namespace rt {

struct CpuAnalyticLightHit {
    bool hit = false;
    double t = 0.0;
    Eigen::Vector3d position = Eigen::Vector3d::Zero();
    Eigen::Vector3d normal = Eigen::Vector3d::UnitZ();
    Eigen::Vector3d radiance = Eigen::Vector3d::Zero();
    int light_index = -1;
};

struct CpuAnalyticLightSample {
    Eigen::Vector3d direction = Eigen::Vector3d::UnitZ();
    Eigen::Vector3d position = Eigen::Vector3d::Zero();
    Eigen::Vector3d normal = Eigen::Vector3d::UnitZ();
    Eigen::Vector3d radiance = Eigen::Vector3d::Zero();
    double distance = 0.0;
    double pdf = 0.0;
    bool infinite = false;
    bool delta = false;
    bool valid = false;
};

class CpuAnalyticLightSampler {
public:
    explicit CpuAnalyticLightSampler(std::vector<AnalyticLightDesc> lights);

    [[nodiscard]] bool empty() const { return lights_.empty(); }
    [[nodiscard]] const std::vector<AnalyticLightDesc>& lights() const { return lights_; }

    [[nodiscard]] CpuAnalyticLightSample sample(const Eigen::Vector3d& surface_point,
        double light_sample, double shape_sample_0, double shape_sample_1) const;
    [[nodiscard]] bool intersect(const Ray& ray, const Interval& ray_t,
        CpuAnalyticLightHit& hit) const;
    [[nodiscard]] double pdf_for_hit(const CpuAnalyticLightHit& hit, const Eigen::Vector3d& origin,
        const Eigen::Vector3d& direction) const;
    [[nodiscard]] double emission_mis_weight(const CpuAnalyticLightHit& hit,
        const Eigen::Vector3d& origin, const Eigen::Vector3d& direction, double bsdf_pdf,
        bool previous_scatter_valid, bool previous_scatter_delta) const;
    [[nodiscard]] Eigen::Vector3d infinite_radiance(const Eigen::Vector3d& direction,
        double bsdf_pdf, bool previous_scatter_valid, bool previous_scatter_delta) const;

private:
    std::vector<AnalyticLightDesc> lights_;
};

} // namespace rt
