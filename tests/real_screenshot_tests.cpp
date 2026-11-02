#include "core/DiffDetector.h"
#include "ui/ImageIO.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <stdexcept>

void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        check(argc == 3, "Expected input and evidence directories.");
        const QDir input(QString::fromLocal8Bit(argv[1]));
        QDir output(QString::fromLocal8Bit(argv[2]));
        check(QDir().mkpath(output.absolutePath()), "Cannot create evidence directory.");
        const auto baseline = pixelguard::loadImage(input.filePath("baseline.png"));
        check(baseline.cols == 2560 && baseline.rows == 1368, "Unexpected screenshot size; review fixture coordinates.");
        pixelguard::saveImage(output.filePath("baseline.png"), baseline);
        QJsonArray records;
        auto record = [&](const QString& name, const cv::Mat& before, const cv::Mat& after,
                          const QString& provenance, bool controlled) {
            auto result = pixelguard::detectDifferences(before, after);
            cv::Mat diff, gray, raw;
            cv::absdiff(before, after, diff);
            cv::cvtColor(diff, gray, cv::COLOR_BGR2GRAY);
            cv::threshold(gray, raw, 20, 255, cv::THRESH_BINARY);
            const int rawCount = cv::countNonZero(raw);
            const int retained = cv::countNonZero(result.mask);
            cv::Mat unexpected;
            cv::bitwise_not(raw, unexpected);
            cv::bitwise_and(unexpected, result.mask, unexpected);
            check(cv::countNonZero(unexpected) == 0, "False changed pixels outside threshold mask.");
            if (name == "identical") {
                check(result.regions.empty() && result.changedRatio == 0, "Identical screenshot must be exactly zero.");
            } else if (controlled) {
                check(!result.regions.empty() && rawCount > 0, "Controlled visual change was missed.");
                check(static_cast<double>(retained) / rawCount >= 0.90, "More than 10% of above-threshold edit pixels missed.");
            }
            pixelguard::saveImage(output.filePath(name + ".png"), after);
            pixelguard::saveImage(output.filePath(name + "_result.png"), result.annotated);
            QJsonArray boxes;
            for (const auto& box : result.regions)
                boxes.append(QJsonArray{box.x, box.y, box.width, box.height});
            records.append(QJsonObject{{"case", name}, {"provenance", provenance},
                {"controlled_edit", controlled}, {"threshold", 20},
                {"regions", static_cast<int>(result.regions.size())}, {"raw_changed_pixels", rawCount},
                {"retained_pixels", retained}, {"changed_percent", result.changedRatio * 100},
                {"above_threshold_coverage", rawCount ? static_cast<double>(retained) / rawCount : 1.0},
                {"boxes_xywh", boxes}});
            std::cout << "PASS " << name.toStdString() << " regions=" << result.regions.size()
                      << " percent=" << result.changedRatio * 100 << " retained=" << retained
                      << '/' << rawCount << '\n';
        };
        record("identical", baseline, baseline, "Unmodified synthetic UI fixture compared with itself", false);

        // These are controlled edits OF a synthetic UI fixture, not independently captured software versions.
        auto color = baseline.clone();
        const cv::Rect button(225, 254, 158, 58);
        for (int y = button.y; y < button.br().y; ++y)
            for (int x = button.x; x < button.br().x; ++x) {
                auto& pixel = color.at<cv::Vec3b>(y, x);
                if (pixel[0] > pixel[2] + 25) std::swap(pixel[0], pixel[2]);
            }
        record("button_color", baseline, color, "Synthetic UI fixture; blue button pixels recolored, text preserved", true);
        auto moved = baseline.clone();
        const auto buttonCopy = baseline(button).clone();
        moved(button).setTo(cv::Scalar(255, 255, 255));
        buttonCopy.copyTo(moved(cv::Rect(button.x + 200, button.y, button.width, button.height)));
        record("button_moved", baseline, moved, "Synthetic UI fixture; button crop translated 200px right", true);
        auto missing = baseline.clone();
        missing(cv::Rect(1060, 380, 370, 315)).setTo(cv::Scalar(255, 255, 255));
        record("element_missing", baseline, missing, "Synthetic UI fixture; sample card erased to white", true);
        auto text = baseline.clone();
        const cv::Rect smallText(1065, 660, 80, 28);
        auto glyphs = baseline(smallText).clone();
        text(smallText).setTo(cv::Scalar(255, 255, 255));
        glyphs.copyTo(text(cv::Rect(smallText.x + 2, smallText.y, smallText.width, smallText.height)));
        record("fine_text", baseline, text, "Synthetic UI fixture; small gray caption text translated 2px horizontally", true);

        for (const auto& name : {"candidate-color.png", "candidate-moved.png", "candidate-missing.png"})
            record(QString::fromLatin1(name).section('.', 0, 0), baseline, pixelguard::loadImage(input.filePath(name)),
                   "Two original, independently captured synthetic UI fixtures; report only, no semantic ground truth", false);
        const QString first = input.filePath(QStringLiteral("QQ20261020-194811测试.png"));
        const QString second = input.filePath(QStringLiteral("QQ20261020-194827测试.png"));
        if (QFile::exists(first) && QFile::exists(second)) {
            const auto before = pixelguard::loadImage(first);
            pixelguard::saveImage(output.filePath("natural_text_before.png"), before);
            record("natural_text_after", before, pixelguard::loadImage(second),
                   "Two private browser screenshots (not distributed), report only; content changes are not semantic controls", false);
        }
        QFile report(output.filePath("results.json"));
        check(report.open(QIODevice::WriteOnly), "Cannot write test report.");
        report.write(QJsonDocument(QJsonObject{{"image_width", 2560}, {"image_height", 1368},
            {"note", "Coverage is against the threshold mask, NOT semantic accuracy or object recall."},
            {"cases", records}}).toJson(QJsonDocument::Indented));
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n'; return 1;
    }
    return 0;
}
