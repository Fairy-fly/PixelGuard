#include "ui/MainWindow.h"
#include "ui/ImageIO.h"
#include "ui/ImagePreview.h"
#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QStyle>
#include <QVBoxLayout>
#include <opencv2/imgproc.hpp>

namespace {
QLabel* label(const QString& text, const char* name)
{
    auto* item = new QLabel(text);
    item->setObjectName(QString::fromLatin1(name));
    item->setTextFormat(Qt::PlainText);
    return item;
}
QImage displayImage(const cv::Mat& bgr)
{
    if (bgr.empty()) return {};
    cv::Mat rgb;
    cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
    return QImage(rgb.data, rgb.cols, rgb.rows, static_cast<qsizetype>(rgb.step),
                  QImage::Format_RGB888).copy();
}
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), lastDirectory_(QDir::homePath())
{
    setObjectName("mainWindow");
    setWindowTitle(QStringLiteral("PixelGuard — 界面视觉差异检测"));
    resize(1180, 800);
    setMinimumSize(920, 680);
    auto* root = new QWidget(this);
    root->setObjectName("page");
    auto* rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    setCentralWidget(root);

    auto* header = new QFrame(root);
    header->setObjectName("header");
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(28, 20, 28, 20);
    auto* brandLayout = new QVBoxLayout;
    brandLayout->setSpacing(4);
    brandLayout->addWidget(label("PixelGuard", "brand"));
    brandLayout->addWidget(label(QStringLiteral("软件界面视觉差异检测 · 对比、定位、验证"), "subtitle"));
    headerLayout->addLayout(brandLayout);
    headerLayout->addStretch();
    headerLayout->addWidget(label(QStringLiteral("Windows  /  OpenCV %1").arg(CV_VERSION), "version"));
    rootLayout->addWidget(header);
    auto* body = new QVBoxLayout;
    body->setContentsMargins(24, 20, 24, 20);
    body->setSpacing(16);
    rootLayout->addLayout(body, 1);

    auto* tools = new QFrame;
    tools->setObjectName("controls");
    auto* toolsLayout = new QHBoxLayout(tools);
    toolsLayout->setContentsMargins(20, 16, 20, 16);
    toolsLayout->setSpacing(14);
    auto* thresholdLayout = new QVBoxLayout;
    thresholdLayout->setSpacing(6);
    auto* thresholdRow = new QHBoxLayout;
    auto* thresholdLabel = label(QStringLiteral("差异阈值"), "sectionLabel");
    thresholdRow->addWidget(thresholdLabel);
    thresholdSlider_ = new QSlider(Qt::Horizontal);
    thresholdSlider_->setObjectName("thresholdSlider");
    thresholdSlider_->setRange(0, 255);
    thresholdSlider_->setValue(20);
    thresholdSlider_->setMinimumWidth(150);
    thresholdSlider_->setAccessibleName(QStringLiteral("差异阈值滑块"));
    thresholdRow->addWidget(thresholdSlider_, 1);
    thresholdSpin_ = new QSpinBox;
    thresholdSpin_->setObjectName("thresholdSpin");
    thresholdSpin_->setRange(0, 255);
    thresholdSpin_->setValue(20);
    thresholdSpin_->setFixedWidth(76);
    thresholdSpin_->setAccessibleName(QStringLiteral("差异阈值"));
    thresholdLabel->setBuddy(thresholdSpin_);
    thresholdRow->addWidget(thresholdSpin_);
    thresholdLayout->addLayout(thresholdRow);
    thresholdLayout->addWidget(label(QStringLiteral("值越小越敏感；修改后需重新检测。"), "hint"));
    toolsLayout->addLayout(thresholdLayout, 1);
    toolsLayout->addSpacing(20);
    detectButton_ = new QPushButton(QStringLiteral("开始检测"));
    detectButton_->setObjectName("detectButton");
    detectButton_->setMinimumWidth(124);
    detectButton_->setToolTip(QStringLiteral("比较同尺寸、已对齐的两张图片"));
    saveButton_ = new QPushButton(QStringLiteral("保存结果"));
    saveButton_->setObjectName("saveButton");
    saveButton_->setMinimumWidth(116);
    saveButton_->setToolTip(QStringLiteral("保存带红框的原始分辨率图片"));
    toolsLayout->addWidget(detectButton_);
    toolsLayout->addWidget(saveButton_);
    body->addWidget(tools);

    auto* previews = new QHBoxLayout;
    previews->setSpacing(16);
    auto createCard = [&](bool baseline) {
        auto* card = new QFrame;
        card->setProperty("role", "previewCard");
        auto* layout = new QVBoxLayout(card);
        layout->setContentsMargins(20, 16, 20, 16);
        layout->setSpacing(10);
        auto* heading = new QHBoxLayout;
        auto* title = label(baseline ? QStringLiteral("基准图片") : QStringLiteral("待检测图片"),
                            baseline ? "baselineTitle" : "candidateTitle");
        title->setProperty("role", "cardTitle");
        heading->addWidget(title);
        heading->addStretch();
        auto* importButton = new QPushButton(baseline ? QStringLiteral("导入基准图") : QStringLiteral("导入待检测图"));
        importButton->setObjectName(baseline ? "importBaselineButton" : "importCandidateButton");
        heading->addWidget(importButton);
        layout->addLayout(heading);
        auto* preview = new ImagePreview(baseline ? QStringLiteral("导入基准图片\nPNG / JPG") :
                                                                  QStringLiteral("导入待检测图片\n检测后将在此显示红框结果"));
        preview->setProperty("role", "imagePreview");
        preview->setObjectName(baseline ? "baselinePreview" : "candidatePreview");
        layout->addWidget(preview, 1);
        auto* info = label(QStringLiteral("尚未导入图片"), baseline ? "baselineInfo" : "candidateInfo");
        info->setProperty("role", "imageInfo");
        info->setWordWrap(true);
        info->setMinimumHeight(40);
        layout->addWidget(info);
        if (baseline) { baselinePreview_ = preview; baselineInfo_ = info; }
        else { candidatePreview_ = preview; candidateInfo_ = info; candidateTitle_ = title; }
        connect(importButton, &QPushButton::clicked, this, [this, baseline] { importImage(baseline); });
        previews->addWidget(card, 1);
    };
    createCard(true);
    createCard(false);
    body->addLayout(previews, 1);

    auto* statistics = new QFrame;
    statistics->setObjectName("statistics");
    auto* statsLayout = new QHBoxLayout(statistics);
    statsLayout->setContentsMargins(20, 14, 20, 14);
    statsLayout->setSpacing(18);
    statsLayout->addWidget(label(QStringLiteral("差异区域"), "regionsCaption"));
    regionValue_ = label(QStringLiteral("—"), "regionValue");
    regionValue_->setProperty("role", "metric");
    statsLayout->addWidget(regionValue_);
    statsLayout->addSpacing(28);
    statsLayout->addWidget(label(QStringLiteral("变化像素"), "ratioCaption"));
    ratioValue_ = label(QStringLiteral("—"), "ratioValue");
    ratioValue_->setProperty("role", "metric");
    statsLayout->addWidget(ratioValue_);
    statsLayout->addStretch();
    statsLayout->addWidget(label(QStringLiteral("红框定位变化 · 原始分辨率导出"), "resultHint"));
    body->addWidget(statistics);
    status_ = label({}, "statusLabel");
    status_->setWordWrap(true);
    status_->setMinimumHeight(42);
    body->addWidget(status_);
    body->addWidget(label(QStringLiteral("仅比较同尺寸、已对齐的图片；不进行自动配准或尺寸拉伸。"), "footer"));

    connect(thresholdSlider_, &QSlider::valueChanged, thresholdSpin_, &QSpinBox::setValue);
    connect(thresholdSpin_, &QSpinBox::valueChanged, thresholdSlider_, &QSlider::setValue);
    connect(thresholdSpin_, &QSpinBox::valueChanged, this, [this] {
        clearResult();
        updateAvailability();
    });
    connect(detectButton_, &QPushButton::clicked, this, [this] { detect(); });
    connect(saveButton_, &QPushButton::clicked, this, [this] { saveResult(); });
    updateAvailability();
}

bool MainWindow::imagesMatch() const
{
    return !baseline_.empty() && !candidate_.empty() && baseline_.size() == candidate_.size();
}
void MainWindow::setStatus(const QString& text, bool error)
{
    status_->setText(text);
    status_->setProperty("error", error);
    status_->style()->unpolish(status_);
    status_->style()->polish(status_);
}
void MainWindow::showError(const QString& title, const QString& message, const QString& detail)
{
    setStatus(message, true);
    QMessageBox box(QMessageBox::Warning, title, message, QMessageBox::Ok, this);
    box.setTextFormat(Qt::PlainText);
    box.setDetailedText(detail);
    box.exec();
}
void MainWindow::clearResult()
{
    hasResult_ = false;
    result_ = {};
    saveButton_->setEnabled(false);
    regionValue_->setText(QStringLiteral("—"));
    ratioValue_->setText(QStringLiteral("—"));
    candidateTitle_->setText(QStringLiteral("待检测图片"));
    candidatePreview_->setImage(displayImage(candidate_));
}
void MainWindow::updateAvailability()
{
    detectButton_->setEnabled(imagesMatch());
    saveButton_->setEnabled(hasResult_);
    if (baseline_.empty() || candidate_.empty())
        setStatus(QStringLiteral("请导入基准图和待检测图，准备开始比较。"));
    else if (!imagesMatch())
        setStatus(QStringLiteral("图片尺寸不同：基准 %1 × %2，待检测 %3 × %4。请导入同尺寸图片。")
            .arg(baseline_.cols).arg(baseline_.rows).arg(candidate_.cols).arg(candidate_.rows), true);
    else
        setStatus(QStringLiteral("图片已就绪 · 阈值 %1。点击“开始检测”。").arg(thresholdSpin_->value()));
}
void MainWindow::importImage(bool baseline)
{
    const QString path = QFileDialog::getOpenFileName(this,
        baseline ? QStringLiteral("导入基准图") : QStringLiteral("导入待检测图"), lastDirectory_,
        QStringLiteral("图片 (*.png *.jpg *.jpeg);;PNG 图片 (*.png);;JPG 图片 (*.jpg *.jpeg)"));
    if (path.isEmpty()) return;
    try {
        cv::Mat loaded = pixelguard::loadImage(path);
        const QImage preview = displayImage(loaded);
        auto* info = baseline ? baselineInfo_ : candidateInfo_;
        info->setText(QStringLiteral("%1\n%2 × %3 像素")
            .arg(QFileInfo(path).fileName()).arg(loaded.cols).arg(loaded.rows));
        info->setToolTip(path);
        if (baseline) {
            baseline_ = std::move(loaded);
            baselinePreview_->setImage(preview);
        } else {
            candidate_ = std::move(loaded);
        }
        lastDirectory_ = QFileInfo(path).absolutePath();
        clearResult();
        updateAvailability();
        if (!baseline_.empty() && !candidate_.empty() && !imagesMatch())
            showError(QStringLiteral("图片尺寸不一致"), status_->text());
    } catch (const std::exception& error) {
        showError(QStringLiteral("导入失败"), QStringLiteral("无法读取所选图片。请检查文件是否有效、可访问且为 8 位 PNG/JPG。原有图片和结果已保留。"),
                  QString::fromUtf8(error.what()));
    }
}
void MainWindow::detect()
{
    if (!imagesMatch()) return;
    clearResult();
    setStatus(QStringLiteral("正在检测，请稍候…"));
    status_->repaint();
    QApplication::setOverrideCursor(Qt::WaitCursor);
    try {
        const auto result = pixelguard::detectDifferences(baseline_, candidate_, {thresholdSpin_->value(), 8});
        const auto preview = displayImage(result.annotated);
        result_ = result;
        hasResult_ = true;
        candidatePreview_->setImage(preview);
        candidateTitle_->setText(QStringLiteral("检测结果 · 红框标记"));
        regionValue_->setText(QString::number(result_.regions.size()));
        ratioValue_->setText(result_.changedRatio == 0 ? QStringLiteral("0%") :
                             QStringLiteral("%1%").arg(result_.changedRatio * 100.0, 0, 'f', 4));
        saveButton_->setEnabled(true);
        setStatus(result_.regions.empty() ? QStringLiteral("检测完成 · 当前阈值下未发现有效差异，可以保存结果。") :
            QStringLiteral("检测完成 · 发现 %1 个差异区域，右图已用红框标记。可保存完整分辨率结果。")
                .arg(result_.regions.size()));
        QApplication::restoreOverrideCursor();
    } catch (const std::exception& error) {
        QApplication::restoreOverrideCursor();
        clearResult();
        showError(QStringLiteral("检测失败"), QStringLiteral("图像检测未完成，请检查图片或尝试较小的图片。"),
                  QString::fromUtf8(error.what()));
    }
}
void MainWindow::saveResult()
{
    if (!hasResult_) return;
    QFileDialog dialog(this, QStringLiteral("保存检测结果"),
        QDir(lastDirectory_).filePath(QStringLiteral("PixelGuard_检测结果.png")),
        QStringLiteral("PNG 图片 (*.png);;JPG 图片 (*.jpg *.jpeg)"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    // Let QFileDialog append the suffix BEFORE its overwrite confirmation.
    dialog.setDefaultSuffix("png");
    connect(&dialog, &QFileDialog::filterSelected, &dialog, [&dialog](const QString& filter) {
        dialog.setDefaultSuffix(filter.startsWith("JPG") ? "jpg" : "png");
    });
    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) return;
    const QString path = dialog.selectedFiles().first();
    try {
        pixelguard::saveImage(path, result_.annotated);
        lastDirectory_ = QFileInfo(path).absolutePath();
        setStatus(QStringLiteral("已保存 %1 × %2 像素结果：%3")
            .arg(result_.annotated.cols).arg(result_.annotated.rows).arg(path));
    } catch (const std::exception& error) {
        showError(QStringLiteral("保存失败"), QStringLiteral("无法保存结果。请检查扩展名（PNG/JPG）、保存位置及写入权限。检测结果已保留，可重试。"),
                  QString::fromUtf8(error.what()));
    }
}
