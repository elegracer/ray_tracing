#pragma once

#include "common.h"

#include <opencv2/opencv.hpp>
#include <fmt/core.h>

struct RTWImage {
    cv::Mat m_f32Mat;
    cv::Mat m_u8Mat;

    RTWImage() = default;
    RTWImage(const std::string& img_filepath) {
        if (load(img_filepath)) {
            return;
        }
        fmt::print("ERROR Constructing RTWImage with img_filepath: {}\n", img_filepath);
    }

    bool load(const std::string& img_filepath) {
        cv::Mat img = cv::imread(img_filepath);
        if (img.empty()) {
            return false;
        }

        cv::cvtColor(img, m_u8Mat, cv::COLOR_BGR2RGB);
        if (m_u8Mat.empty()) {
            return false;
        }

        m_u8Mat.convertTo(m_f32Mat, CV_32FC3, 1 / 255.0);
        if (m_f32Mat.empty()) {
            return false;
        }

        return true;
    }

    Vec3d pixel_data(const int x, const int y) const {
        if (!is_valid()) {
            return {1.0, 0.0, 1.0};
        }

        const int clamped_x = std::clamp(x, 0, width() - 1);
        const int clamped_y = std::clamp(y, 0, height() - 1);

        const cv::Vec3f cv_pixel = m_f32Mat.at<cv::Vec3f>(y, x);

        return {cv_pixel[0], cv_pixel[1], cv_pixel[2]};
    }

    int width() const { return m_u8Mat.cols; }
    int height() const { return m_u8Mat.rows; }

    bool is_valid() const { return !m_f32Mat.empty() && !m_u8Mat.empty(); }
};
