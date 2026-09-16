#ifndef TOUCH_DETECTOR_H
#define TOUCH_DETECTOR_H

#include <opencv2/opencv.hpp>
#include <vector>

#include "PianoLayout.h"

class TouchDetector
{
public:

    TouchDetector();

    void setReferenceDepth(
        const cv::Mat& depth
    );

    bool hasReference() const;

    std::vector<int> detectTouchedKeys(
        const cv::Mat& currentDepth,
        const std::vector<PianoKey>& keys
    ) const;

private:

    cv::Mat referenceDepth;

    int depthDifferenceThreshold;

    int minimumChangedPixels;
};

#endif