#pragma once

#include <Eigen/Core>

#include <string>
#include <vector>

namespace rt {

enum class AnalyticLightType : int {
    sphere = 0,
    disk = 1,
    rect = 2,
    cylinder = 3,
    distant = 4,
    dome = 5,
};

struct AnalyticLightDesc {
    AnalyticLightType type = AnalyticLightType::sphere;
    std::string source_path;
    Eigen::Vector3d position = Eigen::Vector3d::Zero();
    Eigen::Matrix3d local_to_world_linear = Eigen::Matrix3d::Identity();
    Eigen::Vector3d radiance = Eigen::Vector3d::Zero();
    double radius = 0.5;
    double width = 1.0;
    double height = 1.0;
    double length = 1.0;
    double world_area = 0.0;
    double cos_theta_max = 1.0;
    double diffuse = 1.0;
    double specular = 1.0;
    double selection_weight = 0.0;
    double selection_pdf = 0.0;
    double cdf = 0.0;
    bool delta = false;
    bool treat_as_point = false;
    bool treat_as_line = false;
};

void finalize_analytic_light_distribution(std::vector<AnalyticLightDesc>& lights);

} // namespace rt
