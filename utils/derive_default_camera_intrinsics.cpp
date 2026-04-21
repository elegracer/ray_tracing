#include "realtime/camera_models.h"

#include <argparse/argparse.hpp>
#include <fmt/core.h>

#include <exception>
#include <string>

namespace {

rt::CameraModelType parse_model(const std::string& name) {
    if (name == "pinhole32") {
        return rt::CameraModelType::pinhole32;
    }
    if (name == "equi62_lut1d") {
        return rt::CameraModelType::equi62_lut1d;
    }
    throw std::invalid_argument("unsupported camera model");
}

}  // namespace

int main(int argc, char** argv) {
    argparse::ArgumentParser program("derive_default_camera_intrinsics");
    program.add_argument("--model").required();
    program.add_argument("--width").scan<'i', int>().required();
    program.add_argument("--height").scan<'i', int>().required();
    program.add_argument("--hfov-deg").scan<'g', double>().required();

    try {
        program.parse_args(argc, argv);
        const std::string model_name = program.get<std::string>("--model");
        const int width = program.get<int>("--width");
        const int height = program.get<int>("--height");
        const double hfov_deg = program.get<double>("--hfov-deg");
        const rt::DefaultCameraIntrinsics intrinsics =
            rt::derive_default_camera_intrinsics(parse_model(model_name), width, height, hfov_deg);
        fmt::print("model={}\nwidth={}\nheight={}\nhfov_deg={:.12f}\nfx={:.12f}\nfy={:.12f}\ncx={:.12f}\ncy={:.12f}\n",
            model_name,
            width,
            height,
            hfov_deg,
            intrinsics.fx,
            intrinsics.fy,
            intrinsics.cx,
            intrinsics.cy);
        return 0;
    } catch (const std::exception& ex) {
        fmt::print(stderr, "error: {}\n", ex.what());
        return 1;
    }
}
