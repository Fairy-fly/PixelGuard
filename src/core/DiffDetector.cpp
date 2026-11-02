#include "core/DiffDetector.h"

#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <stdexcept>
#include <tuple>

namespace pixelguard {

DiffResult detectDifferences(const cv::Mat& baseline, const cv::Mat& candidate,
                             const DiffOptions& options)
{
    if (baseline.empty() || candidate.empty())
        throw std::invalid_argument("Input image is empty.");
    if (baseline.size() != candidate.size())
        throw std::invalid_argument("Image sizes must match; automatic alignment is unsupported.");
    if (baseline.type() != CV_8UC3 || candidate.type() != CV_8UC3)
        throw std::invalid_argument("Input images must be 8-bit three-channel BGR.");
    if (options.threshold < 0 || options.threshold > 255 || options.minChangedPixels < 1)
        throw std::invalid_argument("Threshold must be 0..255 and minimum changed pixels must be positive.");

    cv::Mat difference, gray, rawMask, closedMask;
    cv::absdiff(baseline, candidate, difference);
    // Convert AFTER absdiff so equal-luminance color changes do not cancel out.
    cv::cvtColor(difference, gray, cv::COLOR_BGR2GRAY);
    cv::threshold(gray, rawMask, options.threshold, 255, cv::THRESH_BINARY);
    cv::morphologyEx(rawMask, closedMask, cv::MORPH_CLOSE,
                     cv::getStructuringElement(cv::MORPH_RECT, {3, 3}));

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(closedMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    DiffResult result;
    result.mask = cv::Mat::zeros(rawMask.size(), CV_8UC1);
    result.annotated = candidate.clone();
    for (const auto& contour : contours) {
        const cv::Rect box = cv::boundingRect(contour);
        // Work within each bounding box to avoid allocating full-image masks per region.
        cv::Mat regionMask = cv::Mat::zeros(box.size(), CV_8UC1);
        std::vector<std::vector<cv::Point>> localContours{contour};
        cv::drawContours(regionMask, localContours, 0, cv::Scalar(255), cv::FILLED,
                         cv::LINE_8, cv::noArray(), 0, -box.tl());
        cv::bitwise_and(regionMask, rawMask(box), regionMask);
        if (cv::countNonZero(regionMask) < options.minChangedPixels)
            continue;
        cv::bitwise_or(result.mask(box), regionMask, result.mask(box));
        result.regions.push_back(box);
    }
    std::sort(result.regions.begin(), result.regions.end(), [](const cv::Rect& a, const cv::Rect& b) {
        return std::tie(a.y, a.x, a.height, a.width) < std::tie(b.y, b.x, b.height, b.width);
    });
    result.changedRatio = static_cast<double>(cv::countNonZero(result.mask)) /
                          static_cast<double>(baseline.total());
    for (const auto& box : result.regions)
        cv::rectangle(result.annotated, box, cv::Scalar(0, 0, 255), 2);
    return result;
}

} // namespace pixelguard
