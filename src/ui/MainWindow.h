#pragma once
#include "core/DiffDetector.h"
#include <QMainWindow>

class ImagePreview;
class QLabel;
class QPushButton;
class QSlider;
class QSpinBox;

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);
private:
    void importImage(bool baseline);
    void detect();
    void saveResult();
    void clearResult();
    void updateAvailability();
    void setStatus(const QString& text, bool error = false);
    void showError(const QString& title, const QString& message, const QString& detail = {});
    bool imagesMatch() const;

    cv::Mat baseline_;
    cv::Mat candidate_;
    pixelguard::DiffResult result_;
    bool hasResult_ = false;
    QString lastDirectory_;
    ImagePreview* baselinePreview_;
    ImagePreview* candidatePreview_;
    QLabel* baselineInfo_;
    QLabel* candidateInfo_;
    QLabel* candidateTitle_;
    QLabel* regionValue_;
    QLabel* ratioValue_;
    QLabel* status_;
    QPushButton* detectButton_;
    QPushButton* saveButton_;
    QSlider* thresholdSlider_;
    QSpinBox* thresholdSpin_;
};
