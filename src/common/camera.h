#pragma once

#include "hittable.h"

#include <fmt/format.h>
#include <icecream.hpp>
#include <opencv2/opencv.hpp>


class Camera {
public:
    Camera() { initialize(); }

    double aspect_ratio = 1.0;  // Ratio of image width over height
    int image_width = 100;      // Rendered image width in pixel count
    int samples_per_pixel = 10; // Count of random samples for each pixel
    cv::Mat img;                // Rendered image as cv::Mat

public:
    void render(const pro::proxy<Hittable>& world) {
        initialize();

        img = cv::Mat(image_height, image_width, CV_8UC3);

        for (int y = 0; y < image_height; y++) {
            for (int x = 0; x < image_width; x++) {
                const Vec3d pixel_center = pixel00_loc + (x * pixel_delta_u) + (y * pixel_delta_v);
                const Vec3d ray_direction = pixel_center - center;

                Vec3d pixel_color = {0.0, 0.0, 0.0};
                for (int sample = 0; sample < samples_per_pixel; ++sample) {
                    Ray ray = get_ray(x, y);
                    pixel_color += ray_color(ray, world);
                }
                pixel_color *= pixel_samples_scale;

                // Translate the [0,1] component values to the byte range [0,255]
                static const Interval intensity(0.000, 0.999);
                const int rbyte = int(256 * intensity.clamp(pixel_color.x()));
                const int gbyte = int(256 * intensity.clamp(pixel_color.y()));
                const int bbyte = int(256 * intensity.clamp(pixel_color.z()));

                img.at<cv::Vec3b>(y, x) = cv::Vec3b(bbyte, gbyte, rbyte);
            }
        }
    }

private:
    int image_height;               // Rendered image height
    double pixel_samples_scale;     // Color scale factor for a sum of pixel samples
    Vec3d center = {0.0, 0.0, 0.0}; // Camera center
    Vec3d pixel_delta_u;            // Offset to pixel to the right
    Vec3d pixel_delta_v;            // Offset to pixel below
    Vec3d pixel00_loc;              // Location of pixel 0, 0

    void initialize() {
        image_height = std::max(int(image_width / aspect_ratio), 1);

        pixel_samples_scale = 1.0 / samples_per_pixel;

        // Determine viewport dimensions.
        const double focal_length = 1.0;
        const double viewport_height = 2.0;
        const double viewport_width = viewport_height * (double(image_width) / image_height);

        // Calculate the vectors across the horizontal and down the vertical viewport edges.
        const Vec3d viewport_u {viewport_width, 0.0, 0.0};
        const Vec3d viewport_v {0.0, -viewport_height, 0.0};

        // Calculate the horizontal and vertical delta vectors from pixel to pixel
        pixel_delta_u = viewport_u / image_width;
        pixel_delta_v = viewport_v / image_height;

        // Calculate the location of the upper left pixel
        const Vec3d viewport_upper_left =
            center - Vec3d {0.0, 0.0, focal_length} - viewport_u / 2 - viewport_v / 2;
        pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);
    }

    Ray get_ray(const int x, const int y) const {
        // Construct a camera ray originating from the origin and directed at randomly
        // sampled point around the pixel location i, j
        const Vec3d offset = sample_square();
        const Vec3d pixel_sample =
            pixel00_loc + ((x + offset.x()) * pixel_delta_u) + ((y + offset.y()) * pixel_delta_v);
        const Vec3d ray_origin = center;
        const Vec3d ray_direction = pixel_sample - ray_origin;

        return Ray(ray_origin, ray_direction);
    }

    Vec3d sample_square() const {
        // Returns the vector to a random point in the [-0.5,-0.5]-[+0.5,+0.5] unit square.
        return Vec3d(random_double() - 0.5, random_double() - 0.5, 0.0);
    }

    Vec3d ray_color(const Ray& ray, const pro::proxy<Hittable>& hittable) {
        HitRecord hit_rec;

        if (hittable->hit(ray, Interval {0, infinity}, hit_rec)) {
            return 0.5 * (hit_rec.normal + Vec3d::Ones());
        }

        const Vec3d unit_direction = ray.direction().normalized();
        const double a = 0.5 * (unit_direction.y() + 1.0);
        return (1.0 - a) * Vec3d {1.0, 1.0, 1.0} + a * Vec3d {0.5, 0.7, 1.0};
    }
};
