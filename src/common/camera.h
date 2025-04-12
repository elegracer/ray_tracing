#pragma once

#include "hittable.h"
#include "color.h"

#include <fmt/core.h>
#include <fmt/format.h>
#include <icecream.hpp>
#include <opencv2/opencv.hpp>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range2d.h>
#include <indicators/block_progress_bar.hpp>
#include <indicators/cursor_control.hpp>

class Camera {
public:
    Camera() { initialize(); }

    double aspect_ratio = 1.0;  // Ratio of image width over height
    int image_width = 100;      // Rendered image width in pixel count
    int samples_per_pixel = 10; // Count of random samples for each pixel
    int max_depth = 10;         // Maximum number of ray bounces into scene

    double vfov = 90;                 // Vertical field of view in degrees
    Vec3d lookfrom = {0.0, 0.0, 0.0}; // Point camera is looking from
    Vec3d lookat = {0.0, 0.0, -1.0};  // Point camera is looking at
    Vec3d vup = {0.0, 1.0, 0.0};      // Camera-relative "up" direction

    double defocus_angle = 0.0; // Variation angle of rays through each pixel
    double focus_dist = 10.0;   // Distance from camera lookfrom point to plane of perfect focus

    cv::Mat img; // Rendered image as cv::Mat

    std::atomic_int rendered_pixel_count;
    int total_pixel_count;

public:
    void render(const pro::proxy<Hittable>& world) {
        initialize();

        img = cv::Mat(image_height, image_width, CV_8UC3);


        // Initialize progress bar
        using namespace indicators;
        show_console_cursor(false);
        BlockProgressBar bar {option::ForegroundColor {Color::white},
            option::FontStyles {std::vector<FontStyle> {FontStyle::bold}},
            option::MaxProgress {total_pixel_count}};

        tbb::parallel_for(tbb::blocked_range2d<int>(0, image_height, 0, image_width),
            [&, this](const tbb::blocked_range2d<int>& range) {
                for (int y = range.rows().begin(), y_end = range.rows().end(); y < y_end; ++y) {
                    for (int x = range.cols().begin(), x_end = range.cols().end(); x < x_end; ++x) {
                        const Vec3d pixel_center =
                            pixel00_loc + (x * pixel_delta_u) + (y * pixel_delta_v);
                        const Vec3d ray_direction = pixel_center - center;

                        Vec3d pixel_color = {0.0, 0.0, 0.0};
                        for (int sample = 0; sample < samples_per_pixel; ++sample) {
                            Ray ray = get_ray(x, y);
                            pixel_color += ray_color(ray, max_depth, world);
                        }
                        pixel_color *= pixel_samples_scale;

                        // Apply a linear to gamma transform for gamma 2
                        pixel_color.x() = linear_to_gamma(pixel_color.x());
                        pixel_color.y() = linear_to_gamma(pixel_color.y());
                        pixel_color.z() = linear_to_gamma(pixel_color.z());

                        // Translate the [0,1] component values to the byte range [0,255]
                        static const Interval intensity(0.000, 0.999);
                        const int rbyte = int(256 * intensity.clamp(pixel_color.x()));
                        const int gbyte = int(256 * intensity.clamp(pixel_color.y()));
                        const int bbyte = int(256 * intensity.clamp(pixel_color.z()));

                        img.at<cv::Vec3b>(y, x) = cv::Vec3b(bbyte, gbyte, rbyte);
                        rendered_pixel_count += 1;
                    }
                }
                const int rendered_pixel_count_val = rendered_pixel_count.load();
                bar.set_option(option::PostfixText {
                    fmt::format("{}/{}", rendered_pixel_count_val, total_pixel_count)});
                bar.set_progress(rendered_pixel_count_val);
            });

        bar.mark_as_completed();
        show_console_cursor(true);
    }

private:
    int image_height;               // Rendered image height
    double pixel_samples_scale;     // Color scale factor for a sum of pixel samples
    Vec3d center = {0.0, 0.0, 0.0}; // Camera center
    Vec3d u, v, w;                  // Camera frame basis vectors
    Vec3d pixel_delta_u;            // Offset to pixel to the right
    Vec3d pixel_delta_v;            // Offset to pixel below
    Vec3d pixel00_loc;              // Location of pixel 0, 0
    Vec3d defocus_disk_u;           // Defocus disk horizontal radius
    Vec3d defocus_disk_v;           // Defocus disk vertical radius

    void initialize() {
        image_height = std::max(int(image_width / aspect_ratio), 1);

        pixel_samples_scale = 1.0 / samples_per_pixel;

        center = lookfrom;

        // Determine viewport dimensions.
        const double theta = deg2rad(vfov);
        const double h = std::tan(0.5 * theta);
        const double viewport_height = 2.0 * h * focus_dist;
        const double viewport_width = viewport_height * (double(image_width) / image_height);

        // Calculate the u,v,w unit basis vectors from the camera coordinate frame.
        w = (lookfrom - lookat).normalized();
        u = vup.cross(w).normalized();
        v = w.cross(u).normalized();

        // Calculate the vectors across the horizontal and down the vertical viewport edges.
        const Vec3d viewport_u = viewport_width * u;
        const Vec3d viewport_v = viewport_height * -v;

        // Calculate the horizontal and vertical delta vectors from pixel to pixel
        pixel_delta_u = viewport_u / image_width;
        pixel_delta_v = viewport_v / image_height;

        // Calculate the location of the upper left pixel
        const Vec3d viewport_upper_left =
            center - (focus_dist * w) - viewport_u / 2 - viewport_v / 2;
        pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);

        // Calculate the camera focus disk basis vectors.
        const double defocus_radius = focus_dist * std::tan(deg2rad(0.5 * defocus_angle));
        defocus_disk_u = u * defocus_radius;
        defocus_disk_v = v * defocus_radius;

        // Initialize rendered pixel count and total pixel count
        total_pixel_count = image_width * image_height;
        rendered_pixel_count = 0;
    }

    Ray get_ray(const int x, const int y) const {
        // Construct a camera ray originating from the defocus disk and directed at randomly
        // sampled point around the pixel location i, j
        const Vec3d offset = sample_square();
        const Vec3d pixel_sample =
            pixel00_loc + ((x + offset.x()) * pixel_delta_u) + ((y + offset.y()) * pixel_delta_v);

        const Vec3d ray_origin = (defocus_angle < 0.0) ? center : defocus_disk_sample();
        const Vec3d ray_direction = pixel_sample - ray_origin;
        const double ray_time = random_double();

        return Ray(ray_origin, ray_direction, ray_time);
    }

    Vec3d sample_square() const {
        // Returns the vector to a random point in the [-0.5,-0.5]-[+0.5,+0.5] unit square.
        return Vec3d(random_double() - 0.5, random_double() - 0.5, 0.0);
    }

    Vec3d defocus_disk_sample() const {
        // Returns a random point in the camera focus disk.
        const Vec3d p = random_in_init_disk();
        return center + (p.x() * defocus_disk_u) + (p.y() * defocus_disk_v);
    }

    Vec3d ray_color(const Ray& ray, const int depth, const pro::proxy<Hittable>& hittable) {
        // If we've exceeded the ray bounce limit, no more light is gathered
        if (depth <= 0) {
            return {0.0, 0.0, 0.0};
        }

        HitRecord hit_rec;

        if (hittable->hit(ray, Interval {0.001, infinity}, hit_rec)) {
            Ray scattered;
            Vec3d attenuation;
            if (hit_rec.mat->scatter(ray, hit_rec, attenuation, scattered)) {
                return attenuation.array() * ray_color(scattered, depth - 1, hittable).array();
            }
            return {0.0, 0.0, 0.0};
        }

        const Vec3d unit_direction = ray.direction().normalized();
        const double a = 0.5 * (unit_direction.y() + 1.0);
        return (1.0 - a) * Vec3d {1.0, 1.0, 1.0} + a * Vec3d {0.5, 0.7, 1.0};
    }
};
