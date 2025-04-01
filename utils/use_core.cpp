#include "common/sphere.h"
#include "core/core.h"

#include <fmt/core.h>
#include <Eigen/Eigen>
#include <cxxopts.hpp>
#include <opencv2/opencv.hpp>

#include "common/common.h"
#include "common/camera.h"
#include "common/hittable_list.h"


int main(int argc, const char* argv[]) {

    std::string output_image_format = "png";

    cxxopts::Options options("use_core", core::get_project_description());
    options                           //
        .set_tab_expansion()          //
        .allow_unrecognised_options() //
        .add_options()                //
        ("output_image_format", "Output Image Format, e.g. jpg, png, bmp etc.",
            cxxopts::value<std::string>(output_image_format));

    const auto result = options.parse(argc, argv);

    if (result.count("output_image_format")) {
        fmt::print("parsed output_image_format: {}\n", output_image_format);
    } else {
        fmt::print("use default output_image_format: {}\n", output_image_format);
    }


    // World
    HittableList world;
    world.add(pro::make_proxy<Hittable, Sphere>(Vec3d {0.0, 0.0, -1.0}, 0.5));
    world.add(pro::make_proxy<Hittable, Sphere>(Vec3d {0.0, -100.5, -1.0}, 100.0));

    pro::proxy<Hittable> world_as_hittable = &world;

    // Camera
    Camera cam;
    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 1280;
    cam.samples_per_pixel = 50;

    // Render
    cam.render(world_as_hittable);

    // cv::imshow("output image", cam.img);
    // cv::waitKey();
    cv::imwrite(fmt::format("output.{}", output_image_format), cam.img);

    return 0;
}
