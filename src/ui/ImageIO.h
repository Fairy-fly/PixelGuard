#pragma once
#include <QString>
#include <opencv2/core.hpp>

namespace pixelguard {
// QFile handles Unicode paths; OpenCV decodes/encodes bytes only.
// Alpha is composited onto white. Errors throw std::runtime_error.
cv::Mat loadImage(const QString& path);
void saveImage(const QString& path, const cv::Mat& bgr);
}
