#include "ui/MainWindow.h"
#include "ui/ImagePreview.h"
#include "ui/ImageViewer.h"
#include "ui/ImageIO.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QTabBar>
#include <QToolButton>
#include <QAbstractButton>
#include <QSplitter>
#include "ui/DesignTokens.h"
#include <QScrollBar>
#include <QWheelEvent>
#include <QPointer>
#include <QDialogButtonBox>
#include <QDir>
#include <QElapsedTimer>
#include <QDynamicPropertyChangeEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontInfo>
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
class ProcessingCapture : public QObject {
public:
    explicit ProcessingCapture(MainWindow& window) : window_(window) { window_.installEventFilter(this); }
    bool captured = false;
protected:
    bool eventFilter(QObject* object, QEvent* event) override {
        if (!captured && object == &window_ && event->type() == QEvent::DynamicPropertyChange &&
            static_cast<QDynamicPropertyChangeEvent*>(event)->propertyName() == "processing" && window_.property("processing").toBool())
            captured = window_.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("ui-detecting-smoke.png"));
        return false;
    }
private:
    MainWindow& window_;
};
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
    auto* button = child<QAbstractButton>(window, name);
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
        const auto* zoom = child<QAbstractButton>(window, name == QStringLiteral("baselinePreview") ? "baselineZoomButton" : "candidateZoomButton");
        require(!QRect(preview->mapTo(&window, QPoint()), preview->size()).intersects(QRect(zoom->mapTo(&window, QPoint()), zoom->size())),
                "Preview overlaps toolbar at current window size.");
        require(preview->height() >= window.height() * 0.50, "Canvas occupies less than half the window height.");
        // Integer scaling permits less than one pixel of rounding on an axis.
        require(std::abs(static_cast<double>(target.width()) * source.height() / source.width() - target.height()) <= 1.1,
                "Preview aspect ratio changed.");
    }
}

void checkViewer(ImagePreview* preview, const QImage& expected)
{
    const auto before = preview->image();
    for (int iteration = 0; iteration < 3; ++iteration) {
        QTest::mouseClick(preview, Qt::LeftButton, Qt::NoModifier, preview->rect().center());
        QApplication::processEvents();
        auto* dialog = preview->window()->findChild<QDialog*>("imageViewer");
        require(dialog && dialog->isVisible(), "Image click did not open viewer.");
        QPointer<QDialog> guard(dialog);
        auto* view = static_cast<ImageView*>(dialog->findChild<QWidget*>("imageView"));
        require(view && view->sceneRect().size() == QSizeF(expected.size()), "Viewer lost original resolution.");
        require(dialog->findChild<QLabel*>("viewerInfo")->text().contains(QString::number(expected.width())), "Viewer metadata missing.");
        require(std::abs(view->transform().m11() - view->transform().m22()) < 0.000001, "Viewer stretched image.");
        QTest::mouseClick(dialog->findChild<QAbstractButton*>("actualSizeButton"), Qt::LeftButton);
        require(view->zoom() == 1.0 && dialog->findChild<QLabel*>("zoomLabel")->text() == "100.0%", "100% zoom failed.");
        require(dialog->findChild<QAbstractButton*>("actualSizeButton")->isChecked() && !dialog->findChild<QAbstractButton*>("fitButton")->isChecked(), "100% selection state incorrect.");
        dialog->resize(850, 600);
        QTest::qWait(30);
        require(view->zoom() == 1.0, "Resize changed manual 100% zoom.");
        preview->openPreview();
        require(preview->window()->findChild<QDialog*>("imageViewer") == dialog && view->zoom() == 1.0,
                "Repeated open replaced viewer or reset manual zoom.");
        const QPoint position = view->viewport()->rect().center();
        QWheelEvent wheel(QPointF(position), QPointF(view->viewport()->mapToGlobal(position)), QPoint(), QPoint(0, 480),
                          Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(view->viewport(), &wheel);
        require(view->zoom() > 1.0, "Wheel zoom failed.");
        require(!dialog->findChild<QAbstractButton*>("actualSizeButton")->isChecked() && !dialog->findChild<QAbstractButton*>("fitButton")->isChecked(), "Manual zoom left a preset selected.");
        const int scrollBefore = view->horizontalScrollBar()->value();
        QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, position);
        QMouseEvent move(QEvent::MouseMove, QPointF(position - QPoint(80, 40)),
            QPointF(view->viewport()->mapToGlobal(position - QPoint(80, 40))), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(view->viewport(), &move);
        QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, position - QPoint(80, 40));
        require(view->horizontalScrollBar()->value() != scrollBefore, "Drag did not pan image.");
        QTest::mouseClick(dialog->findChild<QAbstractButton*>("fitButton"), Qt::LeftButton);
        dialog->resize(600, 420);
        QTest::qWait(30);
        require(view->sceneRect().width() * view->zoom() <= view->viewport()->width() + 1 &&
                view->sceneRect().height() * view->zoom() <= view->viewport()->height() + 1, "Fit after resize failed.");
        if (iteration == 0) {
            const QDir output(QCoreApplication::applicationDirPath());
            require(dialog->findChild<QAbstractButton*>("fitButton")->isChecked(), "Fit selection state incorrect.");
            const QString screenshot = expected.width() > 1000 ? QStringLiteral("ui-viewer-large-smoke.png") :
                dialog->windowTitle().contains(QStringLiteral("红框")) ? QStringLiteral("ui-viewer-result-smoke.png") :
                QStringLiteral("ui-viewer-original-smoke.png");
            require(dialog->grab().save(output.filePath(screenshot)), "Viewer screenshot failed.");
            QTest::mouseClick(dialog->findChild<QAbstractButton*>("closePreviewButton"), Qt::LeftButton);
        } else QTest::keyClick(view, Qt::Key_Escape);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        require(guard.isNull(), "Viewer not deleted after close/Esc.");
    }
    require(preview->image() == before && before == expected, "Preview changed image data.");
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
        ProcessingCapture processingCapture(window);
        window.show();
        require(QTest::qWaitForWindowExposed(&window), "Window not exposed.");
        std::cout << "DPI devicePixelRatio=" << window.devicePixelRatioF()
                  << " logicalDpi=" << window.logicalDpiX() << '\n';
        std::cout << "FONT family=" << QFontInfo(child<QLabel>(window, "statusLabel")->font()).family().toStdString()
                  << " pixels=" << child<QLabel>(window, "statusLabel")->font().pixelSize() << '\n';
        require(!child<QPushButton>(window, "detectButton")->isEnabled(), "Detect enabled before import.");
        require(!child<QPushButton>(window, "saveButton")->isEnabled(), "Save enabled before result.");
        require(child<QSpinBox>(window, "thresholdSpin")->value() == 20, "Default threshold mismatch.");
        auto* leftPreview = static_cast<ImagePreview*>(child<QWidget>(window, "baselinePreview"));
        auto* rightPreview = static_cast<ImagePreview*>(child<QWidget>(window, "candidatePreview"));
        require(window.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("ui-empty-smoke.png")), "Empty screenshot failed.");
        QTest::mouseClick(leftPreview, Qt::LeftButton);
        leftPreview->openPreview();
        require(!window.findChild<QDialog*>("imageViewer"), "Empty viewer opened.");
        require(!child<QAbstractButton>(window, "baselineZoomButton")->isEnabled(), "Empty zoom enabled.");
        require(fileAction(window, "importBaselineButton", {}, true) == 0, "Cancel showed an error.");
        require(text(window, "baselineInfo") == QStringLiteral("尚未导入图片"), "Cancel changed initial image.");
        std::cout << "PASS initial controls, default threshold and cancel import\n";

        require(fileAction(window, "importBaselineButton", baselinePath) == 0, "Baseline import error.");
        require(!child<QPushButton>(window, "detectButton")->isEnabled(), "Detect enabled with one image.");
        require(fileAction(window, "importCandidateButton", candidatePath) == 0, "Candidate import error.");
        require(child<QPushButton>(window, "detectButton")->isEnabled(), "Detect disabled for valid pair.");
        require(!child<QPushButton>(window, "saveButton")->isEnabled(), "Save enabled without detection.");
        require(text(window, "resultState") == QStringLiteral("准备就绪"), "Ready state headline missing.");
        require(window.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("ui-ready-smoke.png")), "Ready screenshot failed.");
        click(window, "detectButton");
        require(processingCapture.captured && !window.property("processing").toBool(), "Processing state not exposed or not restored.");
        const auto expected = pixelguard::detectDifferences(baseline, candidate);
        require(text(window, "regionValue") == QString::number(expected.regions.size()), "UI region count mismatch.");
        require(text(window, "resultState") == QStringLiteral("发现差异"), "Difference state headline missing.");
        require(text(window, "ratioValue") == QStringLiteral("%1%").arg(expected.changedRatio * 100, 0, 'f', 4), "UI ratio mismatch.");
        require(child<QPushButton>(window, "saveButton")->isEnabled(), "Save disabled after detection.");
        auto* mode = child<QTabBar>(window, "resultMode");
        require(mode->isEnabled() && mode->currentIndex() == 1, "Detection did not select result view.");
        const QImage annotation = rightPreview->image();
        const QString ratioBeforeSwitch = text(window, "ratioValue");
        const QString statusBeforeSwitch = text(window, "statusLabel");
        checkViewer(rightPreview, annotation);
        QTest::mouseClick(mode, Qt::LeftButton, Qt::NoModifier, mode->tabRect(0).center());
        require(mode->currentIndex() == 0, "Original tab click failed.");
        require(rightPreview->image() != annotation, "Original/result switch unchanged.");
        const QImage original = rightPreview->image();
        checkViewer(rightPreview, original);
        for (int i = 0; i < 10; ++i) { mode->setCurrentIndex(1); mode->setCurrentIndex(0); }
        require(text(window, "ratioValue") == ratioBeforeSwitch && text(window, "statusLabel") == statusBeforeSwitch &&
                child<QPushButton>(window, "saveButton")->isEnabled(), "Switch changed detection state.");
        QTest::mouseClick(mode, Qt::LeftButton, Qt::NoModifier, mode->tabRect(1).center());
        require(mode->currentIndex() == 1, "Result tab click failed.");
        require(rightPreview->image() == annotation, "Switch lost annotation cache.");
        checkViewer(leftPreview, leftPreview->image());
        click(window, "baselineZoomButton");
        require(window.findChild<QDialog*>("imageViewer"), "Zoom entry did not open viewer.");
        window.findChild<QDialog*>("imageViewer")->close();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        std::cout << "PASS empty preview, click/toolbar entry, repeat close/Esc, wheel, drag, fit, 100%, switch consistency\n";
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
        click(window, "candidateZoomButton");
        QPointer<QDialog> oldViewer(window.findChild<QDialog*>("imageViewer"));
        slider->setFocus();
        QTest::keyClick(slider, Qt::Key_Right);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        require(oldViewer.isNull() && !mode->isEnabled() && mode->currentIndex() == 0 && rightPreview->image() == original,
                "Threshold retained stale viewer or result mode.");
        require(slider->value() == 21 && child<QSpinBox>(window, "thresholdSpin")->value() == 21, "Slider to spinbox sync failed.");
        require(!child<QPushButton>(window, "saveButton")->isEnabled(), "Threshold left stale result savable.");
        require(text(window, "regionValue") == QStringLiteral("—"), "Threshold left stale statistics.");
        require(text(window, "candidateTitle") == QStringLiteral("待检测图片"), "Threshold left result title.");
        typeThreshold(window, 255);
        click(window, "detectButton");
        require(text(window, "regionValue") == "0" && text(window, "ratioValue") == "0%", "Threshold 255 mismatch.");
        require(text(window, "resultState") == QStringLiteral("未发现有效差异"), "Zero-difference state missing.");
        typeThreshold(window, 0);
        click(window, "detectButton");
        require(text(window, "regionValue") != "0", "Threshold zero ignored changes.");
        typeThreshold(window, 20);
        click(window, "detectButton");
        std::cout << "PASS threshold controls in both directions, stale-result clearing and 0/255\n";

        require(fileAction(window, "importBaselineButton", baselinePath) == 0, "Baseline replacement failed.");
        require(!mode->isEnabled() && mode->currentIndex() == 0 && rightPreview->image() == original,
                "Input replacement retained result view.");
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
        std::cout << "LAYOUT window=" << window.width() << 'x' << window.height()
                  << " canvas=" << rightPreview->width() << 'x' << rightPreview->height()
                  << " image=" << rightPreview->imageRect().width() << 'x' << rightPreview->imageRect().height() << '\n';
        require(child<QSlider>(window, "thresholdSlider")->width() <= 160, "Threshold expanded across workbench.");
        auto* split = child<QSplitter>(window, "imageSplitter");
        const QPoint handleCenter = split->handle(1)->rect().center();
        QTest::mousePress(split->handle(1), Qt::LeftButton, Qt::NoModifier, handleCenter);
        QMouseEvent splitterMove(QEvent::MouseMove, QPointF(handleCenter + QPoint(80, 0)),
            QPointF(split->handle(1)->mapToGlobal(handleCenter + QPoint(80, 0))), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(split->handle(1), &splitterMove);
        QTest::mouseRelease(split->handle(1), Qt::LeftButton, Qt::NoModifier, handleCenter + QPoint(80, 0));
        QApplication::processEvents();
        require(split->sizes()[0] != split->sizes()[1] && split->sizes()[0] >= 340 && split->sizes()[1] >= 340,
                "Splitter drag did not resize panes safely.");
        checkAspect(window);
        split->setSizes({640, 640});
        window.resize(920, 640);
        QTest::qWait(40);
        checkAspect(window);
        const auto longPath = temp.filePath(QStringLiteral("较长的文件名_").repeated(12) + QStringLiteral(".png"));
        pixelguard::saveImage(longPath, baseline);
        require(fileAction(window, "importCandidateButton", longPath) == 0, "Long filename import failed.");
        require(window.width() == 920 && child<QLabel>(window, "candidateInfo")->text().contains(QFileInfo(longPath).fileName()),
                "Long filename expanded window or lost accessible text.");
        require(window.grab().save(output.filePath("ui-long-name-smoke.png")), "Long-name screenshot failed.");
        std::cout << "PASS compact toolbar, splitter drag, minimum 920x640, long-name elision and semantic states\n";
        std::cout << "PASS resize at minimum and larger window, aspect ratio and screenshot capture\n";
        ImagePreview largePreview(QStringLiteral("large"));
        QImage large(4096, 2048, QImage::Format_RGB888);
        large.fill(QColor("#ECECF7"));
        largePreview.resize(800, 500);
        largePreview.setImage(large, QStringLiteral("大图 4096.png"));
        largePreview.show();
        require(QTest::qWaitForWindowExposed(&largePreview), "Large fixture not exposed.");
        checkViewer(&largePreview, large);
        require(largePreview.image().constBits() == large.constBits(), "Display unnecessarily copied QImage storage.");
        std::cout << "PASS 4096x2048 large-image viewer and shared display storage\n";
        largePreview.close();
        if (argc > 1) {
            const QDir real(QString::fromLocal8Bit(argv[1]));
            const auto realBaseline = real.filePath("baseline.png");
            const auto realCandidate = real.filePath("candidate-color.png");
            require(fileAction(window, "importBaselineButton", realBaseline) == 1, "Synthetic baseline replacement should flag the temporary size mismatch.");
            require(fileAction(window, "importCandidateButton", realCandidate) == 0, "Synthetic candidate import failed.");
            window.resize(1280, 850);
            click(window, "detectButton");
            const auto expectedSynthetic = pixelguard::detectDifferences(pixelguard::loadImage(realBaseline), pixelguard::loadImage(realCandidate));
            require(text(window, "regionValue") == QString::number(expectedSynthetic.regions.size()), "Synthetic image UI count mismatch.");
            checkAspect(window);
            require(window.grab().save(output.filePath("ui-workbench-real.png")), "Synthetic workbench screenshot failed.");
            click(window, "candidateZoomButton");
            auto* realViewer = window.findChild<QDialog*>("imageViewer");
            require(realViewer && realViewer->grab().save(output.filePath("ui-viewer-real.png")), "Synthetic viewer screenshot failed.");
            realViewer->close();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            std::cout << "PASS synthetic fixture pair through workbench and viewer\n";
        }
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
    return 0;
}
