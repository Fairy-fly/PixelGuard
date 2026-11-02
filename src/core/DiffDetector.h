#pragma once

#include <opencv2/core.hpp>
#include <vector>

namespace pixelguard {

struct DiffOptions {
    int threshold = 20;       // Pixel is changed when gray(absdiff) > threshold.
    int minChangedPixels = 8; // Count original changed pixels, not rectangle area.
};

struct DiffResult {
    std::vector<cv::Rect> regions;
    cv::Mat mask;             // CV_8UC1: retained original changed pixels only.
    double changedRatio = 0; // Fraction [0, 1]; UI multiplies by 100 for percent.
    cv::Mat annotated;        // Independent BGR copy of candidate with red boxes.
};

// Accepts nonempty, same-size CV_8UC3 BGR images. Does not modify inputs.
// Throws std::invalid_argument for invalid input/options; OpenCV errors propagate.
DiffResult detectDifferences(const cv::Mat& baseline, const cv::Mat& candidate,
                             const DiffOptions& options = {});

} // namespace pixelguard
