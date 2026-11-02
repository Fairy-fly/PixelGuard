#include "ui/MainWindow.h"
#include "ui/ImagePreview.h"
#include "ui/ImageIO.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <opencv2/imgproc.hpp>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}
template<class Widget> Widget* child(MainWindow& window, const char* name)
{
    auto* widget = window.findChild<Widget*>(QString::fromLatin1(name));
    if (!widget) throw std::runtime_error(std::string("Missing widget: ") + name);
    return widget;
}
QString text(MainWindow& window, const char* name) { return child<QLabel>(window, name)->text(); }
void click(MainWindow& window, const char* name)
{
    auto* button = child<QPushButton>(window, name);
    require(button->isEnabled(), "Cannot click a disabled button.");
    QTest::mouseClick(button, Qt::LeftButton);
    QApplication::processEvents();
}

// Drives real QFileDialog widgets via Qt events. Only tests disable native OS dialogs.
// Message boxes are also clicked, so validation follows the production button paths.
int fileAction(MainWindow& window, const char* button, const QString& path = {},
               bool cancel = false, const QString& filter = {})
{
    bool dialogSeen = false;
    bool timedOut = false;
    int messages = 0;
    QElapsedTimer elapsed;
    elapsed.start();
    QTimer driver;
    driver.setInterval(15);
    QObject::connect(&driver, &QTimer::timeout, &window, [&] {
        auto* active = QApplication::activeModalWidget();
        if (elapsed.elapsed() > 5000) {
            timedOut = true;
            if (active) active->close();
            return;
        }
        if (auto* dialog = qobject_cast<QFileDialog*>(active); dialog && !dialogSeen) {
            dialogSeen = true;
            if (cancel) { dialog->reject(); return; }
            dialog->setDirectory(QFileInfo(path).absolutePath());
            if (!filter.isEmpty()) {
                dialog->selectNameFilter(filter);
                // Emulate the signal produced when a user chooses a filter.
                QMetaObject::invokeMethod(dialog, "filterSelected", Qt::DirectConnection, Q_ARG(QString, filter));
            }
            dialog->selectFile(QFileInfo(path).fileName());
            // QFileDialog::accept is protected, but remains a normal Qt slot.
            // Accept can open a nested overwrite prompt. Keep the polling timer
            // out of that call stack so it can click the confirmation as well.
            QTimer::singleShot(0, dialog, [dialog] {
                QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
            });
        } else if (auto* box = qobject_cast<QMessageBox*>(active)) {
            ++messages;
            if (auto* ok = box->button(QMessageBox::Ok)) QTest::mouseClick(ok, Qt::LeftButton);
            else if (auto* yes = box->button(QMessageBox::Yes)) QTest::mouseClick(yes, Qt::LeftButton);
            else box->reject();
        }
    });
    driver.start();
    click(window, button);
    driver.stop();
    require(!timedOut && dialogSeen, "File dialog interaction failed or timed out.");
    return messages;
}

void typeThreshold(MainWindow& window, int value)
{
    auto* spin = child<QSpinBox>(window, "thresholdSpin");
    spin->setFocus();
    QTest::keyClick(spin, Qt::Key_A, Qt::ControlModifier);
    QTest::keyClicks(spin, QString::number(value));
    QTest::keyClick(spin, Qt::Key_Enter);
    require(spin->value() == value, "Spinbox keyboard editing failed.");
    require(child<QSlider>(window, "thresholdSlider")->value() == value, "Spinbox to slider sync failed.");
}

void checkAspect(MainWindow& window)
{
    // ImagePreview has no Q_OBJECT: retrieve by object name then use the known base type.
    for (const char* name : {"baselinePreview", "candidatePreview"}) {
        auto* preview = static_cast<ImagePreview*>(child<QWidget>(window, name));
        const auto source = preview->imageSize();
        const auto target = preview->imageRect();
        require(!source.isEmpty() && !target.isEmpty(), "Preview image is missing.");
        require(preview->rect().contains(target), "Preview exceeds its bounds.");
        // Integer scaling permits less than one pixel of rounding on an axis.
        require(std::abs(static_cast<double>(target.width()) * source.height() / source.width() - target.height()) <= 1.1,
                "Preview aspect ratio changed.");
    }
}
}

int main(int argc, char** argv)
{
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    try {
        require(applyTheme(app), "Theme failed to load.");
        QTemporaryDir temp(QDir::tempPath() + QStringLiteral("/PixelGuard_界面测试_XXXXXX"));
        require(temp.isValid(), "Temporary directory failed.");
        const auto baselinePath = temp.filePath(QStringLiteral("基准.png"));
        const auto candidatePath = temp.filePath(QStringLiteral("待检测.png"));
        const auto mismatchPath = temp.filePath(QStringLiteral("尺寸不同.png"));
        const auto corruptPath = temp.filePath(QStringLiteral("损坏.png"));
        const auto outputPath = temp.filePath(QStringLiteral("红框结果.png"));
        cv::Mat baseline(600, 1000, CV_8UC3, cv::Scalar(245, 245, 245));
        cv::rectangle(baseline, {60, 70, 880, 55}, cv::Scalar(59, 38, 37), cv::FILLED);
        cv::putText(baseline, "PixelGuard / UI comparison", {80, 106}, cv::FONT_HERSHEY_SIMPLEX,
                    0.8, cv::Scalar(255, 255, 255), 2);
        cv::rectangle(baseline, {60, 165, 410, 345}, cv::Scalar(255, 255, 255), cv::FILLED);
        cv::rectangle(baseline, {500, 165, 440, 345}, cv::Scalar(255, 255, 255), cv::FILLED);
        cv::rectangle(baseline, {100, 210, 220, 60}, cv::Scalar(155, 92, 89), cv::FILLED);
        cv::putText(baseline, "Continue", {125, 248}, cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 2);
        cv::putText(baseline, "Status: Ready", {540, 225}, cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(59, 38, 37), 1);
        auto candidate = baseline.clone();
        candidate(cv::Rect(100, 210, 220, 60)).setTo(cv::Scalar(94, 126, 199));
        cv::putText(candidate, "Continue", {125, 248}, cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 2);
        pixelguard::saveImage(baselinePath, baseline);
        pixelguard::saveImage(candidatePath, candidate);
        pixelguard::saveImage(mismatchPath, cv::Mat(200, 200, CV_8UC3, cv::Scalar(255, 255, 255)));
        QFile corrupt(corruptPath);
        require(corrupt.open(QIODevice::WriteOnly), "Corrupt fixture failed.");
        corrupt.write("invalid image"); corrupt.close();

        MainWindow window;
        window.show();
        require(QTest::qWaitForWindowExposed(&window), "Window not exposed.");
        std::cout << "DPI devicePixelRatio=" << window.devicePixelRatioF()
                  << " logicalDpi=" << window.logicalDpiX() << '\n';
        require(!child<QPushButton>(window, "detectButton")->isEnabled(), "Detect enabled before import.");
        require(!child<QPushButton>(window, "saveButton")->isEnabled(), "Save enabled before result.");
        require(child<QSpinBox>(window, "thresholdSpin")->value() == 20, "Default threshold mismatch.");
        require(fileAction(window, "importBaselineButton", {}, true) == 0, "Cancel showed an error.");
        require(text(window, "baselineInfo") == QStringLiteral("尚未导入图片"), "Cancel changed initial image.");
        std::cout << "PASS initial controls, default threshold and cancel import\n";

        require(fileAction(window, "importBaselineButton", baselinePath) == 0, "Baseline import error.");
        require(!child<QPushButton>(window, "detectButton")->isEnabled(), "Detect enabled with one image.");
        require(fileAction(window, "importCandidateButton", candidatePath) == 0, "Candidate import error.");
        require(child<QPushButton>(window, "detectButton")->isEnabled(), "Detect disabled for valid pair.");
        require(!child<QPushButton>(window, "saveButton")->isEnabled(), "Save enabled without detection.");
        click(window, "detectButton");
        const auto expected = pixelguard::detectDifferences(baseline, candidate);
        require(text(window, "regionValue") == QString::number(expected.regions.size()), "UI region count mismatch.");
        require(text(window, "ratioValue") == QStringLiteral("%1%").arg(expected.changedRatio * 100, 0, 'f', 4), "UI ratio mismatch.");
        require(child<QPushButton>(window, "saveButton")->isEnabled(), "Save disabled after detection.");
        require(fileAction(window, "saveButton", outputPath) == 0, "Save failed.");
        const auto saved = pixelguard::loadImage(outputPath);
        require(saved.size() == candidate.size(), "Saved thumbnail instead of full resolution.");
        require(cv::norm(saved, expected.annotated, cv::NORM_INF) == 0, "Saved annotation differs from algorithm.");
        std::cout << "PASS real import, detect, UI statistics and Chinese-path full-resolution PNG save\n";

        const auto noSuffix = temp.filePath(QStringLiteral("无扩展名"));
        require(fileAction(window, "saveButton", noSuffix) == 0 && QFileInfo::exists(noSuffix + ".png"),
                "PNG default suffix failed.");
        require(fileAction(window, "saveButton", noSuffix) == 1, "Default-suffix overwrite confirmation missing.");
        const auto noJpgSuffix = temp.filePath(QStringLiteral("无扩展名JPG"));
        require(fileAction(window, "saveButton", noJpgSuffix, false, QStringLiteral("JPG 图片 (*.jpg *.jpeg)")) == 0 &&
                QFileInfo::exists(noJpgSuffix + ".jpg"), "JPG default suffix failed.");
        std::cout << "PASS default PNG/JPG suffix and overwrite confirmation before saving\n";

        const auto countBefore = text(window, "regionValue");
        const auto statusBefore = text(window, "statusLabel");
        require(fileAction(window, "importCandidateButton", {}, true) == 0, "Cancel import showed error.");
        require(text(window, "regionValue") == countBefore && text(window, "statusLabel") == statusBefore,
                "Cancel import altered results or status.");
        require(fileAction(window, "saveButton", {}, true) == 0, "Cancel save showed error.");
        require(text(window, "statusLabel") == statusBefore, "Cancel save changed status.");
        require(fileAction(window, "importCandidateButton", corruptPath) == 1, "Corrupt image warning missing.");
        require(text(window, "regionValue") == countBefore && child<QPushButton>(window, "saveButton")->isEnabled(),
                "Failed import cleared valid result.");
        require(fileAction(window, "saveButton", temp.filePath("result.invalid")) == 1, "Invalid save warning missing.");
        require(child<QPushButton>(window, "saveButton")->isEnabled(), "Failed save cleared result.");
        std::cout << "PASS cancel preservation, corrupt import and save error/retry state\n";

        auto* slider = child<QSlider>(window, "thresholdSlider");
        slider->setFocus();
        QTest::keyClick(slider, Qt::Key_Right);
        require(slider->value() == 21 && child<QSpinBox>(window, "thresholdSpin")->value() == 21, "Slider to spinbox sync failed.");
        require(!child<QPushButton>(window, "saveButton")->isEnabled(), "Threshold left stale result savable.");
        require(text(window, "regionValue") == QStringLiteral("—"), "Threshold left stale statistics.");
        require(text(window, "candidateTitle") == QStringLiteral("待检测图片"), "Threshold left result title.");
        typeThreshold(window, 255);
        click(window, "detectButton");
        require(text(window, "regionValue") == "0" && text(window, "ratioValue") == "0%", "Threshold 255 mismatch.");
        typeThreshold(window, 0);
        click(window, "detectButton");
        require(text(window, "regionValue") != "0", "Threshold zero ignored changes.");
        typeThreshold(window, 20);
        click(window, "detectButton");
        std::cout << "PASS threshold controls in both directions, stale-result clearing and 0/255\n";

        require(fileAction(window, "importBaselineButton", baselinePath) == 0, "Baseline replacement failed.");
        require(!child<QPushButton>(window, "saveButton")->isEnabled(), "Baseline replacement retained result.");
        require(fileAction(window, "importCandidateButton", mismatchPath) == 1, "Size mismatch warning missing.");
        require(!child<QPushButton>(window, "detectButton")->isEnabled() &&
                !child<QPushButton>(window, "saveButton")->isEnabled(), "Mismatch allowed detect/save.");
        require(text(window, "statusLabel").contains(QStringLiteral("尺寸不同")), "Size mismatch status missing.");
        require(fileAction(window, "importCandidateButton", baselinePath) == 0, "Same-image replacement failed.");
        click(window, "detectButton");
        require(text(window, "regionValue") == "0" && text(window, "ratioValue") == "0%", "Identical images not zero.");
        require(child<QPushButton>(window, "saveButton")->isEnabled(), "Identical valid result not savable.");
        std::cout << "PASS image replacement invalidation, size mismatch and identical-image zero result\n";

        const auto jpegPath = temp.filePath(QStringLiteral("读写测试.jpg"));
        pixelguard::saveImage(jpegPath, baseline);
        require(fileAction(window, "importCandidateButton", jpegPath) == 0, "JPG UI import failed.");
        click(window, "detectButton");
        const auto jpgOutput = temp.filePath(QStringLiteral("导出测试.jpg"));
        require(fileAction(window, "saveButton", jpgOutput) == 0, "JPG UI save failed.");
        require(pixelguard::loadImage(jpgOutput).size() == baseline.size(), "JPG export dimensions mismatch.");
        std::cout << "PASS JPG interface import and full-resolution export\n";

        require(fileAction(window, "importCandidateButton", candidatePath) == 0, "Final candidate import failed.");
        click(window, "detectButton");
        checkAspect(window);
        window.resize(920, 680);
        QTest::qWait(80);
        checkAspect(window);
        const QDir output(QCoreApplication::applicationDirPath());
        require(window.grab().save(output.filePath("ui-minimum-smoke.png")), "Minimum screenshot failed.");
        window.resize(1280, 850);
        QTest::qWait(80);
        checkAspect(window);
        require(window.grab().save(output.filePath("ui-result-smoke.png")), "Result screenshot failed.");
        std::cout << "PASS resize at minimum and larger window, aspect ratio and screenshot capture\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
    return 0;
}
