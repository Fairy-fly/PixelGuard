#include "ui/ImageIO.h"
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <stdexcept>
#include <vector>

namespace pixelguard {
cv::Mat loadImage(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        throw std::runtime_error("Cannot open image: " + file.errorString().toStdString());
    const QByteArray data = file.readAll();
    if (file.error() != QFileDevice::NoError || data.isEmpty())
        throw std::runtime_error("Cannot read image or file is empty.");
    const std::vector<uchar> bytes(data.begin(), data.end());
    cv::Mat decoded = cv::imdecode(bytes, cv::IMREAD_UNCHANGED);
    if (decoded.empty() || decoded.depth() != CV_8U)
        throw std::runtime_error("Invalid image; only 8-bit PNG/JPG images are supported.");
    cv::Mat bgr;
    if (decoded.channels() == 1) {
        cv::cvtColor(decoded, bgr, cv::COLOR_GRAY2BGR);
    } else if (decoded.channels() == 3) {
        bgr = decoded;
    } else if (decoded.channels() == 4) {
        bgr.create(decoded.size(), CV_8UC3);
        for (int y = 0; y < decoded.rows; ++y) {
            const auto* src = decoded.ptr<cv::Vec4b>(y);
            auto* dst = bgr.ptr<cv::Vec3b>(y);
            for (int x = 0; x < decoded.cols; ++x) {
                const int alpha = src[x][3];
                for (int c = 0; c < 3; ++c)
                    dst[x][c] = static_cast<uchar>((src[x][c] * alpha + 255 * (255 - alpha) + 127) / 255);
            }
        }
    } else {
        throw std::runtime_error("Unsupported channel count.");
    }
    return bgr;
}

void saveImage(const QString& path, const cv::Mat& bgr)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (bgr.empty() || bgr.type() != CV_8UC3)
        throw std::runtime_error("No valid BGR image to save.");
    if (suffix != "png" && suffix != "jpg" && suffix != "jpeg")
        throw std::runtime_error("Save format must be PNG or JPG.");
    std::vector<uchar> bytes;
    if (!cv::imencode(("." + suffix).toStdString(), bgr, bytes))
        throw std::runtime_error("Image encoding failed.");
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<qint64>(bytes.size())) != static_cast<qint64>(bytes.size()) ||
        !file.commit())
        throw std::runtime_error("Image saving failed: " + file.errorString().toStdString());
}
} // namespace pixelguard
