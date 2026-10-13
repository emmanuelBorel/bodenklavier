
#ifndef ARUCO_CALIBRATION_H
#define ARUCO_CALIBRATION_H

#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>

#include <array>
#include <map>
#include <vector>
#include <string>

class ArucoCalibration {
public:
    explicit ArucoCalibration(
        const std::string& configPath,
        int requiredFrames = 30
    );

    bool loadReference();

    // Retourne true lorsqu'une nouvelle calibration
    // vient d'etre verrouillee.
    bool processFrame(const cv::Mat& bgr);

    void reset();

    bool isCalibrated() const;

    std::vector<cv::Point2f> getCorners() const;
    std::array<int, 4> getMarkerMisses() const;

    int getCollectedFrames() const;
    int getRequiredFrames() const;
    int getTotalFrames() const;
    int getIncompleteFrames() const;

private:
    std::string configPath_;
    int requiredFrames_;

    cv::aruco::ArucoDetector detector_;

    std::map<int, cv::Point2f> references_;

    std::array<std::vector<cv::Point2f>, 4> samples_;

    std::vector<cv::Point2f> lockedCorners_;

    int collectedFrames_ = 0;
    bool calibrated_ = false;
    int totalFrames_ = 0;
    int incompleteFrames_ = 0;

    static float median(std::vector<float> values);
    std::array<int, 4>  markerMisses_ = {0, 0, 0, 0};
};

#endif
