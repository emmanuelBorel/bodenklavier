#ifndef MAT_CALIBRATION_H
#define MAT_CALIBRATION_H

#include <opencv2/opencv.hpp>
#include <vector>

class MatCalibration {
    public:
    MatCalibration( float lengthCm = 255.0f, float widthCm = 80.0f);

    void addCorner (const cv:: Point2f & point);
    void reset();

    bool isCalibrated() const;

    const std:: vector<cv::Point2f>& getCorners() const;

    cv:: Mat computeHomography (int outputWidth = 800, int outputHeight = 2550);


    private:

    float matLengthCm;
    float matWidthCm;

    std::vector<cv::Point2f> corners;
};

#endif