#include "ui/MainWindow.h"
#include "ui/ImageIO.h"
#include "ui/ImagePreview.h"
#include <QApplication>
#include <QTabBar>
#include <QSplitter>
#include <QToolButton>
#include "ui/DesignTokens.h"
#include "ui/WorkbenchWidgets.h"
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
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
    setWindowTitle(QStringLiteral("PixelGuard — 图像比较工作台"));
    resize(1280, 820);
    setMinimumSize(920, 640);
    auto* root = new QWidget(this);
    root->setObjectName("page");
    root->setFocusPolicy(Qt::StrongFocus);
    root->setFocus();
    auto* rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    setCentralWidget(root);

    auto* header = new QFrame(root);
    header->setObjectName("appBar");
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(design::Margin, design::Small, design::Margin, design::Small);
    headerLayout->setSpacing(design::Gap);
    auto* mark = new QLabel;
    mark->setPixmap(workbenchIcon(Glyph::Compare, QColor(design::Accent)).pixmap(22, 22));
    headerLayout->addWidget(mark);
    headerLayout->addWidget(label("PixelGuard", "brand"));
    headerLayout->addWidget(label(QStringLiteral("/  图像比较"), "workspaceTitle"));
    headerLayout->addStretch();
    headerLayout->addWidget(label(QStringLiteral("本地处理 · 原始分辨率"), "privacyNote"));
    rootLayout->addWidget(header);

    auto* tools = new QFrame;
    tools->setObjectName("controls");
    auto* toolsLayout = new QHBoxLayout(tools);
    toolsLayout->setContentsMargins(design::Margin, design::Gap, design::Margin, design::Gap);
    toolsLayout->setSpacing(design::Gap);
    auto* thresholdLabel = label(QStringLiteral("差异阈值"), "sectionLabel");
    toolsLayout->addWidget(thresholdLabel);
    thresholdSlider_ = new QSlider(Qt::Horizontal);
    thresholdSlider_->setObjectName("thresholdSlider");
    thresholdSlider_->setRange(0, 255);
    thresholdSlider_->setValue(20);
    thresholdSlider_->setFixedWidth(144);
    thresholdSlider_->setAccessibleName(QStringLiteral("差异阈值滑块"));
    thresholdSlider_->setToolTip(QStringLiteral("0–255；值越小越敏感。调整后旧结果失效，需重新检测。"));
    toolsLayout->addWidget(thresholdSlider_);
    thresholdSpin_ = new QSpinBox;
    thresholdSpin_->setObjectName("thresholdSpin");
    thresholdSpin_->setRange(0, 255);
    thresholdSpin_->setValue(20);
    thresholdSpin_->setButtonSymbols(QAbstractSpinBox::NoButtons);
    thresholdSpin_->setAlignment(Qt::AlignCenter);
    thresholdSpin_->setFixedSize(60, design::Control);
    thresholdSpin_->setAccessibleName(QStringLiteral("差异阈值"));
    thresholdSpin_->setToolTip(thresholdSlider_->toolTip());
    thresholdLabel->setBuddy(thresholdSpin_);
    toolsLayout->addWidget(thresholdSpin_);
    toolsLayout->addWidget(label(QStringLiteral("越小越敏感"), "thresholdHint"));
    toolsLayout->addStretch();
    detectButton_ = new QPushButton(QStringLiteral("开始检测"));
    detectButton_->setObjectName("detectButton");
    detectButton_->setIcon(workbenchIcon(Glyph::Compare, QColor(design::Raised)));
    detectButton_->setFixedHeight(design::Control);
    detectButton_->setToolTip(QStringLiteral("比较同尺寸、已对齐的两张图片"));
    saveButton_ = new QPushButton(QStringLiteral("保存结果"));
    saveButton_->setObjectName("saveButton");
    saveButton_->setIcon(workbenchIcon(Glyph::Export));
    saveButton_->setFixedHeight(design::Control);
    saveButton_->setToolTip(QStringLiteral("保存带红框的原始分辨率图片；不受当前查看模式影响"));
    toolsLayout->addWidget(detectButton_);
    toolsLayout->addWidget(saveButton_);
    rootLayout->addWidget(tools);

    auto* split = new QSplitter(Qt::Horizontal);
    split->setObjectName("imageSplitter");
    split->setChildrenCollapsible(false);
    split->setHandleWidth(design::Unit);
    auto createPane = [&](bool baseline) {
        auto* pane = new QFrame;
        pane->setProperty("role", "imagePane");
        pane->setMinimumWidth(340);
        auto* layout = new QVBoxLayout(pane);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        auto* fileHeader = new QFrame;
        fileHeader->setProperty("role", "fileHeader");
        auto* fileLayout = new QVBoxLayout(fileHeader);
        fileLayout->setContentsMargins(design::Margin, design::Gap, design::Margin, design::Gap);
        fileLayout->setSpacing(design::Small);
        auto* identity = new QHBoxLayout;
        identity->setSpacing(design::Small);
        auto* role = label(baseline ? "A" : "B", baseline ? "baselineBadge" : "candidateBadge");
        role->setProperty("role", "identityBadge");
        role->setAlignment(Qt::AlignCenter);
        role->setFixedSize(22,22);
        identity->addWidget(role);
        auto* title = label(baseline ? QStringLiteral("基准图片") : QStringLiteral("待检测图片"),
                            baseline ? "baselineTitle" : "candidateTitle");
        title->setProperty("role", "paneTitle");
        identity->addWidget(title);
        identity->addStretch();
        auto* import = new QPushButton(baseline ? QStringLiteral("导入基准图") : QStringLiteral("导入待检测图"));
        import->setObjectName(baseline ? "importBaselineButton" : "importCandidateButton");
        import->setIcon(workbenchIcon(Glyph::Import));
        import->setProperty("kind", "quiet");
        import->setFixedHeight(design::Compact);
        identity->addWidget(import);
        fileLayout->addLayout(identity);
        auto* info = new ElidedLabel(QStringLiteral("尚未导入图片"));
        info->setObjectName(baseline ? "baselineInfo" : "candidateInfo");
        info->setProperty("role", "fileInfo");
        info->setTextFormat(Qt::PlainText);
        info->setFixedHeight(20);
        fileLayout->addWidget(info);
        layout->addWidget(fileHeader);

        auto* viewBar = new QFrame;
        viewBar->setProperty("role", "viewBar");
        auto* viewLayout = new QHBoxLayout(viewBar);
        viewLayout->setContentsMargins(design::Margin, design::Unit, design::Gap, design::Unit);
        viewLayout->setSpacing(design::Small);
        if (baseline) {
            viewLayout->addWidget(label(QStringLiteral("参考原图"), "baselineViewHint"));
        } else {
            resultMode_ = new QTabBar;
            resultMode_->setObjectName("resultMode");
            resultMode_->addTab(QStringLiteral("待检测原图"));
            resultMode_->addTab(QStringLiteral("红框结果"));
            resultMode_->setDrawBase(false);
            resultMode_->setExpanding(false);
            resultMode_->setEnabled(false);
            resultMode_->setAccessibleName(QStringLiteral("右侧显示内容"));
            resultMode_->setToolTip(QStringLiteral("仅切换显示，不重新检测。导出始终保存红框结果。"));
            viewLayout->addWidget(resultMode_);
            connect(resultMode_, &QTabBar::currentChanged, this, [this] { updateCandidatePreview(); });
        }
        viewLayout->addStretch();
        auto* zoom = new QToolButton;
        zoom->setText(QStringLiteral("放大查看"));
        zoom->setIcon(workbenchIcon(Glyph::Expand));
        zoom->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        zoom->setObjectName(baseline ? "baselineZoomButton" : "candidateZoomButton");
        zoom->setAccessibleName(baseline ? QStringLiteral("放大基准图片") : QStringLiteral("放大当前待检测图片"));
        zoom->setToolTip(QStringLiteral("打开独立预览 · 滚轮缩放 · 拖动查看"));
        zoom->setFixedHeight(design::Compact);
        zoom->setEnabled(false);
        viewLayout->addWidget(zoom);
        layout->addWidget(viewBar);
        auto* preview = new ImagePreview(baseline ? QStringLiteral("导入基准图片") : QStringLiteral("导入待检测图片"));
        preview->setObjectName(baseline ? "baselinePreview" : "candidatePreview");
        preview->setProperty("side", baseline ? "baseline" : "candidate");
        layout->addWidget(preview, 1);
        connect(zoom, &QToolButton::clicked, preview, &ImagePreview::openPreview);
        connect(import, &QPushButton::clicked, this, [this, baseline] { importImage(baseline); });
        if (baseline) { baselinePreview_ = preview; baselineInfo_ = info; baselineZoom_ = zoom; }
        else { candidatePreview_ = preview; candidateInfo_ = info; candidateTitle_ = title; candidateZoom_ = zoom; }
        split->addWidget(pane);
    };
    createPane(true);
    createPane(false);
    split->setSizes({640,640});
    rootLayout->addWidget(split, 1);

    auto* results = new QFrame;
    results->setObjectName("resultBar");
    auto* resultLayout = new QHBoxLayout(results);
    resultLayout->setContentsMargins(design::Margin, design::Gap, design::Margin, design::Gap);
    resultLayout->setSpacing(design::Large);
    auto metric = [&](const QString& caption, const char* valueName) {
        auto* group = new QVBoxLayout;
        group->setSpacing(design::Unit);
        auto* captionLabel = label(caption, "metricCaption");
        captionLabel->setProperty("role", "metricCaption");
        group->addWidget(captionLabel);
        auto* value = label(QStringLiteral("—"), valueName);
        value->setProperty("role", "metric");
        value->setProperty("result", "empty");
        group->addWidget(value);
        resultLayout->addLayout(group);
        return value;
    };
    regionValue_ = metric(QStringLiteral("差异区域"), "regionValue");
    ratioValue_ = metric(QStringLiteral("变化像素占比"), "ratioValue");
    auto* separator = new QFrame;
    separator->setProperty("role", "verticalSeparator");
    separator->setFixedWidth(1);
    resultLayout->addWidget(separator);
    status_ = label({}, "statusLabel");
    status_->setWordWrap(true);
    auto* resultStatus = new QVBoxLayout;
    resultStatus->setSpacing(design::Unit);
    stateLabel_ = label({}, "resultState");
    resultStatus->addWidget(stateLabel_);
    resultStatus->addWidget(status_);
    resultLayout->addLayout(resultStatus, 1);
    rootLayout->addWidget(results);
    auto* footer = new QFrame;
    footer->setObjectName("statusBar");
    auto* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(design::Margin, design::Unit, design::Margin, design::Unit);
    footerLayout->addWidget(label(QStringLiteral("仅比较同尺寸、已对齐的图片"), "footer"));
    footerLayout->addStretch();
    footerLayout->addWidget(label(QStringLiteral("PNG / JPG  ·  本地检测"), "formatHint"));
    rootLayout->addWidget(footer);

    connect(thresholdSlider_, &QSlider::valueChanged, thresholdSpin_, &QSpinBox::setValue);
    connect(thresholdSpin_, &QSpinBox::valueChanged, thresholdSlider_, &QSlider::setValue);
    connect(thresholdSpin_, &QSpinBox::valueChanged, this, [this] { clearResult(); updateAvailability(); });
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
    const bool processing = text.startsWith(QStringLiteral("正在检测"));
    stateLabel_->setText(error ? QStringLiteral("需要处理") : processing ? QStringLiteral("正在检测") :
        hasResult_ ? (result_.changedRatio > 0 ? QStringLiteral("发现差异") : QStringLiteral("未发现有效差异")) :
        imagesMatch() ? QStringLiteral("准备就绪") : QStringLiteral("等待图片"));
    stateLabel_->setProperty("state", error ? "error" : processing ? "processing" :
        hasResult_ ? (result_.changedRatio > 0 ? "difference" : "same") : "idle");
    stateLabel_->style()->unpolish(stateLabel_); stateLabel_->style()->polish(stateLabel_);
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
    resultDisplay_ = {};
    resultMode_->setEnabled(false);
    resultMode_->setCurrentIndex(0);
    saveButton_->setEnabled(false);
    regionValue_->setText(QStringLiteral("—"));
    ratioValue_->setText(QStringLiteral("—"));
    for (auto* value : {regionValue_, ratioValue_}) {
        value->setProperty("result", "empty");
        value->style()->unpolish(value); value->style()->polish(value);
    }
    updateCandidatePreview();
    baselineZoom_->setEnabled(!baseline_.empty());
    candidateZoom_->setEnabled(!candidate_.empty());
    // Close any snapshot based on the previous inputs or threshold.
    baselinePreview_->setImage(baselinePreview_->image(), baselineName_);
}
void MainWindow::updateCandidatePreview()
{
    const bool annotated = hasResult_ && resultMode_->currentIndex() == 1;
    candidateTitle_->setText(QStringLiteral("待检测图片"));
    candidatePreview_->setImage(annotated ? resultDisplay_ : candidateDisplay_,
        candidateName_ + (annotated ? QStringLiteral(" · 红框标注结果") : QString()));
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
        info->setText(QStringLiteral("%1  ·  %2 × %3 px")
            .arg(QFileInfo(path).fileName()).arg(loaded.cols).arg(loaded.rows));
        info->setToolTip(path);
        if (baseline) {
            baselineName_ = QFileInfo(path).fileName();
            baseline_ = std::move(loaded);
            baselinePreview_->setImage(preview, baselineName_);
        } else {
            candidateName_ = QFileInfo(path).fileName();
            candidateDisplay_ = preview;
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
    auto processing = [this](bool busy) {
        detectButton_->setText(busy ? QStringLiteral("检测中…") : QStringLiteral("开始检测"));
        detectButton_->setEnabled(!busy && imagesMatch());
        thresholdSlider_->setEnabled(!busy);
        thresholdSpin_->setEnabled(!busy);
        findChild<QPushButton*>("importBaselineButton")->setEnabled(!busy);
        findChild<QPushButton*>("importCandidateButton")->setEnabled(!busy);
        setProperty("processing", busy);
    };
    setStatus(QStringLiteral("正在检测，请稍候…"));
    processing(true);
    repaint();
    QApplication::setOverrideCursor(Qt::WaitCursor);
    try {
        const auto result = pixelguard::detectDifferences(baseline_, candidate_, {thresholdSpin_->value(), 8});
        const auto preview = displayImage(result.annotated);
        result_ = result;
        hasResult_ = true;
        resultDisplay_ = preview;
        resultMode_->setEnabled(true);
        resultMode_->setCurrentIndex(1);
        regionValue_->setText(QString::number(result_.regions.size()));
        ratioValue_->setText(result_.changedRatio == 0 ? QStringLiteral("0%") :
                             QStringLiteral("%1%").arg(result_.changedRatio * 100.0, 0, 'f', 4));
        saveButton_->setEnabled(true);
        for (auto* value : {regionValue_, ratioValue_}) {
            value->setProperty("result", result_.changedRatio > 0 ? "difference" : "same");
            value->style()->unpolish(value); value->style()->polish(value);
        }
        setStatus(result_.regions.empty() ? QStringLiteral("检测完成 · 当前阈值下未发现有效差异，可以保存结果。") :
            QStringLiteral("检测完成 · 发现 %1 个差异区域，右图已用红框标记。可保存完整分辨率结果。")
                .arg(result_.regions.size()));
        QApplication::restoreOverrideCursor();
        processing(false);
    } catch (const std::exception& error) {
        QApplication::restoreOverrideCursor();
        processing(false);
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
