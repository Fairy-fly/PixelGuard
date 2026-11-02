#include "ui/ImageIO.h"
#include "core/DiffDetector.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <stdexcept>

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
template<class Action> void expectError(Action action)
{
    try { action(); } catch (const std::exception&) { return; }
    throw std::runtime_error("Expected image IO failure.");
}
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        QTemporaryDir dir(QDir::tempPath() + QStringLiteral("/PixelGuard_中文_XXXXXX"));
        require(dir.isValid(), "Cannot create temporary test directory.");
        const auto png = dir.filePath(QStringLiteral("检测结果.png"));
        cv::Mat source(80, 120, CV_8UC3, cv::Scalar(10, 80, 180));
        pixelguard::saveImage(png, source);
        auto loaded = pixelguard::loadImage(png);
        require(cv::norm(source, loaded, cv::NORM_INF) == 0, "Unicode PNG round trip mismatch.");
        std::cout << "PASS Chinese path PNG save/load exact round trip\n";
        const auto jpg = dir.filePath(QStringLiteral("测试.jpg"));
        pixelguard::saveImage(jpg, source);
        require(pixelguard::loadImage(jpg).size() == source.size(), "JPG size mismatch.");
        std::cout << "PASS Chinese path JPG save/load\n";

        const auto alphaPath = dir.filePath(QStringLiteral("透明.png"));
        cv::Mat alpha(10, 10, CV_8UC4, cv::Scalar(0, 0, 0, 0));
        alpha.at<cv::Vec4b>(0, 0) = cv::Vec4b(0, 0, 0, 128);
        std::vector<uchar> bytes; cv::imencode(".png", alpha, bytes);
        QFile file(alphaPath); require(file.open(QIODevice::WriteOnly), "Alpha fixture open failed.");
        require(file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<qint64>(bytes.size())) ==
                static_cast<qint64>(bytes.size()), "Alpha fixture write failed."); file.close();
        auto white = pixelguard::loadImage(alphaPath);
        require(white.at<cv::Vec3b>(1, 1) == cv::Vec3b(255, 255, 255), "Transparent pixel is not white.");
        require(white.at<cv::Vec3b>(0, 0) == cv::Vec3b(127, 127, 127), "Partial alpha mismatch.");
        std::cout << "PASS alpha compositing onto white\n";

        const auto corrupt = dir.filePath(QStringLiteral("损坏.png"));
        QFile bad(corrupt); require(bad.open(QIODevice::WriteOnly), "Corrupt fixture open failed.");
        bad.write("not an image"); bad.close();
        expectError([&] { pixelguard::loadImage(corrupt); });
        expectError([&] { pixelguard::loadImage(dir.filePath("missing.png")); });
        expectError([&] { pixelguard::saveImage(dir.filePath("missing/out.png"), source); });
        expectError([&] { pixelguard::saveImage(dir.filePath("out.txt"), source); });
        expectError([&] { pixelguard::saveImage(png, {}); });
        std::cout << "PASS corrupt/missing file and save errors\n";

        auto changed = source.clone(); changed(cv::Rect(10, 10, 20, 20)).setTo(cv::Scalar(255, 255, 255));
        auto result = pixelguard::detectDifferences(source, changed);
        pixelguard::saveImage(png, result.annotated);
        require(cv::norm(pixelguard::loadImage(png), result.annotated, cv::NORM_INF) == 0,
                "Annotated export mismatch.");
        std::cout << "PASS annotated result export and reload\n";

        // Existing screenshots are only self-comparison inputs, not labeled change groups.
        if (app.arguments().size() > 1) {
            QDir originals(app.arguments().at(1));
            const auto files = originals.entryList({"*.png", "*.jpg", "*.jpeg"}, QDir::Files);
            require(!files.isEmpty(), "No original screenshot inputs found.");
            for (const auto& name : files) {
                const auto image = pixelguard::loadImage(originals.filePath(name));
                const auto same = pixelguard::detectDifferences(image, image);
                require(same.regions.empty() && same.changedRatio == 0, "Screenshot self-comparison failed.");
            }
            std::cout << "PASS " << files.size() << " existing screenshots: load and self-comparison\n";
        }
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n'; return 1;
    }
    return 0;
}
