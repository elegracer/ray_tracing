#pragma once

#include "hittable.h"

#include <fmt/format.h>
#include <icecream.hpp>
#include <opencv2/opencv.hpp>


class Camera {
public:
    Camera() { initialize(); }

    double aspect_ratio = 1.0; // Ratio of image width over height
    int image_width = 100;     // Rendered image width in pixel count
    cv::Mat img;               // Rendered image as cv::Mat

public:
    void render(const pro::proxy<Hittable>& world) {
        initialize();

        img = cv::Mat(image_height, image_width, CV_8UC3);

        for (int y = 0; y < image_height; y++) {
            for (int x = 0; x < image_width; x++) {
                const Vec3d pixel_center = pixel00_loc + (x * pixel_delta_u) + (y * pixel_delta_v);
                const Vec3d ray_direction = pixel_center - center;

                Ray ray(center, ray_direction);

                const Vec3i color = scale_normalized_color(ray_color(ray, world), 256);

                img.at<cv::Vec3b>(y, x) = cv::Vec3b(color.z(), color.y(), color.x());
            }
        }
    }

private:
    int image_height;               // Rendered image height
    Vec3d center = {0.0, 0.0, 0.0}; // Camera center
    Vec3d pixel_delta_u;            // Offset to pixel to the right
    Vec3d pixel_delta_v;            // Offset to pixel below
    Vec3d pixel00_loc;              // Location of pixel 0, 0

    void initialize() {
        image_height = std::max(int(image_width / aspect_ratio), 1);

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

    Vec3i scale_normalized_color(const Vec3d& color_normalized, const int scale) {
        return (color_normalized * ((double)scale - 0.001))
            .cast<int>()
            .array()
            .max(0)
            .min(scale - 1)
            .matrix();
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
