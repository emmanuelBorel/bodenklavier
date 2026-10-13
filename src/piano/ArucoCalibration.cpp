
#include "ArucoCalibration.h"

#include <algorithm>
#include <cmath>
#include <iostream>

ArucoCalibration::ArucoCalibration(
    const std::string& configPath,
    int requiredFrames
)
    : configPath_(configPath),
      requiredFrames_(std::max(1, requiredFrames)),
      detector_(
          cv::aruco::getPredefinedDictionary(
              cv::aruco::DICT_4X4_50
          )
      ) {}

bool ArucoCalibration::loadReference() {
    cv::FileStorage fs(configPath_, cv::FileStorage::READ);

    if (!fs.isOpened()) {
        std::cerr << "Reference ArUco introuvable : "
                  << configPath_ << "\n";
        return false;
    }

    std::map<int, cv::Point2f> loaded;

    cv::FileNode nodes = fs["corners"];

    if (nodes.type() != cv::FileNode::SEQ) {
        return false;
    }

    for (const auto& node : nodes) {
        int id = static_cast<int>(node["id"]);

        float x = static_cast<float>(
            static_cast<double>(node["local_x"])
        );

        float y = static_cast<float>(
            static_cast<double>(node["local_y"])
        );

        if (id < 0 || id > 3 ||
            !std::isfinite(x) ||
            !std::isfinite(y)) {
            return false;
        }

        loaded[id] = cv::Point2f(x, y);
    }

    if (loaded.size() != 4) {
        std::cerr << "Reference ArUco incomplete.\n";
        return false;
    }

    references_ = std::move(loaded);
    reset();

    std::cout << "Reference ArUco chargee : "
              << configPath_ << "\n";

    return true;
}

float ArucoCalibration::median(std::vector<float> values) {
    std::sort(values.begin(), values.end());

    const size_t n = values.size();

    if (n % 2 == 1) {
        return values[n / 2];
    }

    return (values[n / 2 - 1] + values[n / 2]) * 0.5f;
}

bool ArucoCalibration::processFrame(const cv::Mat& bgr) {
    if (calibrated_ || references_.size() != 4 ||bgr.empty()) 
        return false;
        //compter chaque image analysée
        ++totalFrames_;
    

    // La detection doit se faire sur l'image
    // retournee horizontalement, comme dans
    // aruco_auto_test.cpp.
    cv::Mat flipped;
    cv::flip(bgr, flipped, 1);

    std::vector<int> ids;
    std::vector<std::vector<cv::Point2f>> markers;

    detector_.detectMarkers(flipped, markers, ids);

    std::map<int, std::vector<cv::Point2f>> detected;

    const float width = static_cast<float>(bgr.cols);

    for (size_t i = 0; i < ids.size(); ++i) {
        if (references_.count(ids[i]) == 0) {
            continue;
        }

        for (auto& p : markers[i]) {
            p.x = width - 1.0f - p.x;
        }

        detected[ids[i]] = markers[i];
    }

    for (int id = 0; id < 4; ++id)
    {
        if (detected.count(id) == 0)
        {
            ++markerMisses_[id];
        }
    }

    if (detected.size() != 4) {
        ++incompleteFrames_;
        return false;
    }

    // Convention : ID 0, 1, 2, 3.
    // Meme association que la reference enregistree.
    std::array<cv::Point2f, 4> currentCorners;

    const std::vector<cv::Point2f> local = {
        {0, 0},
        {1, 0},
        {1, 1},
        {0, 1}
    };

    for (int id = 0; id < 4; ++id) {
        cv::Mat H = cv::getPerspectiveTransform(
            local,
            detected.at(id)
        );

        std::vector<cv::Point2f> projected;

        cv::perspectiveTransform(
            std::vector<cv::Point2f>{references_.at(id)},
            projected,
            H
        );

        if (projected.empty() ||
            !std::isfinite(projected[0].x) ||
            !std::isfinite(projected[0].y)) {
            return false;
        }

        currentCorners[id] = projected[0];
    }

    for (int id = 0; id < 4; ++id) {
        samples_[id].push_back(currentCorners[id]);
    }

    ++collectedFrames_;

    if (collectedFrames_ < requiredFrames_) {
        return false;
    }

    lockedCorners_.clear();

    for (int id = 0; id < 4; ++id) {
        std::vector<float> xs;
        std::vector<float> ys;

        for (const auto& p : samples_[id]) {
            xs.push_back(p.x);
            ys.push_back(p.y);
        }

        lockedCorners_.emplace_back(
            median(xs),
            median(ys)
        );
    }

    calibrated_ = true;

    std::cout
        << "Calibration ArUco verrouillee apres "
        << collectedFrames_
        << " images valides.\n";

    return true;
}

void ArucoCalibration::reset() {
    for (auto& sample : samples_) {
        sample.clear();
    }

    lockedCorners_.clear();
    collectedFrames_ = 0;
    calibrated_ = false;

    //Reinitialiser les compteurs de diagnostic
    totalFrames_ = 0;
    incompleteFrames_ = 0;

    markerMisses_.fill(0);

}

bool ArucoCalibration::isCalibrated() const {
    return calibrated_;
}

std::vector<cv::Point2f>
ArucoCalibration::getCorners() const {
    return lockedCorners_;
}

int ArucoCalibration::getCollectedFrames() const {
    return collectedFrames_;
}

int ArucoCalibration::getRequiredFrames() const {
    return requiredFrames_;
}

int ArucoCalibration::getTotalFrames() const
{
    return totalFrames_;
}

int ArucoCalibration::getIncompleteFrames() const
{
    return incompleteFrames_;
}

std::array<int, 4> ArucoCalibration::getMarkerMisses() const
{
    return markerMisses_;
}