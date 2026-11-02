#include "core/DiffDetector.h"
#include <opencv2/imgproc.hpp>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
void expectInvalid(const std::function<void()>& action)
{
    try { action(); } catch (const std::invalid_argument&) { return; }
    throw std::runtime_error("Expected invalid_argument.");
}
void ratioIs(const pixelguard::DiffResult& result, int pixels, int total)
{
    require(cv::countNonZero(result.mask) == pixels, "Changed pixel count mismatch.");
    require(std::abs(result.changedRatio - static_cast<double>(pixels) / total) < 1e-12,
            "Changed ratio mismatch.");
}
}

int main()
{
    int failures = 0;
    auto test = [&](const char* name, const std::function<void()>& action) {
        try { action(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) {
            ++failures; std::cerr << "FAIL " << name << ": " << error.what() << '\n';
        }
    };
    using pixelguard::detectDifferences;
    const cv::Mat base(100, 160, CV_8UC3, cv::Scalar(240, 240, 240));
    const cv::Rect button(20, 20, 30, 15);

    test("identical images: zero regions and zero percent", [&] {
        const auto result = detectDifferences(base, base);
        require(result.regions.empty(), "Identical image has regions.");
        ratioIs(result, 0, 16000);
        require(cv::norm(base, result.annotated, cv::NORM_INF) == 0, "Identical annotation changed.");
    });
    test("button color change: exact box, ratio, red annotation, unchanged inputs", [&] {
        cv::Mat changed = base.clone();
        changed(button).setTo(cv::Scalar(20, 60, 180));
        const auto copy = changed.clone();
        const auto result = detectDifferences(base, changed);
        require(result.regions.size() == 1 && result.regions[0] == button, "Wrong button box.");
        ratioIs(result, button.area(), 16000);
        require(result.annotated.at<cv::Vec3b>(button.tl()) == cv::Vec3b(0, 0, 255), "Box is not red.");
        require(cv::norm(changed, copy, cv::NORM_INF) == 0, "Input was modified.");
    });
    test("element movement: old and new locations detected", [&] {
        auto before = base.clone(); auto after = base.clone();
        const cv::Rect moved(90, 20, 30, 15);
        before(button).setTo(cv::Scalar(0, 0, 0));
        after(moved).setTo(cv::Scalar(0, 0, 0));
        const auto result = detectDifferences(before, after);
        require(result.regions.size() == 2 && result.regions[0] == button && result.regions[1] == moved,
                "Movement must detect both locations.");
        ratioIs(result, 2 * button.area(), 16000);
    });
    test("missing element: exact original location", [&] {
        auto before = base.clone(); before(button).setTo(cv::Scalar(0, 0, 0));
        const auto result = detectDifferences(before, base);
        require(result.regions.size() == 1 && result.regions[0] == button, "Missing element box mismatch.");
        ratioIs(result, button.area(), 16000);
    });
    test("isolated noise rejected; thin text retained", [&] {
        auto changed = base.clone();
        changed.at<cv::Vec3b>(70, 100) = cv::Vec3b(0, 0, 0);
        cv::line(changed, {10, 10}, {10, 21}, cv::Scalar(0, 0, 0), 1);
        const auto result = detectDifferences(base, changed);
        require(result.regions.size() == 1, "Thin stroke lost or noise retained.");
        ratioIs(result, 12, 16000);
    });
    test("closing connects gaps without inflating changed ratio", [&] {
        auto changed = base.clone();
        changed(cv::Rect(10, 10, 2, 5)).setTo(cv::Scalar(0, 0, 0));
        changed(cv::Rect(13, 10, 2, 5)).setTo(cv::Scalar(0, 0, 0));
        const auto result = detectDifferences(base, changed);
        require(result.regions.size() == 1, "Closing did not join nearby strokes.");
        ratioIs(result, 20, 16000);
    });
    test("threshold boundary and adjustable sensitivity", [&] {
        auto changed = base.clone(); changed(button).setTo(cv::Scalar(220, 220, 220));
        require(detectDifferences(base, changed).regions.empty(), "Threshold must be strictly greater.");
        require(detectDifferences(base, changed, {19, 8}).regions.size() == 1, "Threshold adjustment failed.");
        require(detectDifferences(base, changed, {255, 8}).regions.empty(), "Threshold 255 should reject all.");
        require(detectDifferences(base, changed, {0, 8}).regions.size() == 1, "Threshold zero failed.");
    });
    test("empty, size, type and parameter errors", [&] {
        expectInvalid([&] { detectDifferences({}, base); });
        expectInvalid([&] { detectDifferences(base, {}); });
        expectInvalid([&] { detectDifferences(base, cv::Mat(10, 10, CV_8UC3)); });
        expectInvalid([&] { detectDifferences(base, cv::Mat(base.size(), CV_8UC1)); });
        expectInvalid([&] { detectDifferences(base, base, {-1, 8}); });
        expectInvalid([&] { detectDifferences(base, base, {256, 8}); });
        expectInvalid([&] { detectDifferences(base, base, {20, 0}); });
    });
    return failures == 0 ? 0 : 1;
}
