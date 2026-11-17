
#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>
#include <iostream>
#include <vector>
#include <array>

int main() {
    const std::string directory = "config/calibration/";

    cv::FileStorage fs(directory + "rgb_reference.yml",
                       cv::FileStorage::READ);

    if (!fs.isOpened()) {
        std::cerr << "Erreur : calibration RGB introuvable.\n";
        return 1;
    }

    std::vector<cv::Point2f> matCorners;
    int width = 0, height = 0;

    fs["corners"] >> matCorners;
    fs["rectified_width"] >> width;
    fs["rectified_height"] >> height;
    fs.release();

    if (matCorners.size() != 4 || width <= 0 || height <= 0) {
        std::cerr << "Calibration RGB invalide.\n";
        return 1;
    }

    cv::Mat image = cv::imread(
        directory + "rgb_aruco_reference.png"
    );

    if (image.empty()) {
        std::cerr << "Image de reference introuvable.\n";
        return 1;
    }

    std::vector<cv::Point2f> destination = {
        {0.f, 0.f},
        {float(width - 1), 0.f},
        {float(width - 1), float(height - 1)},
        {0.f, float(height - 1)}
    };

    cv::Mat H = cv::getPerspectiveTransform(
        matCorners, destination
    );

    auto dictionary = cv::aruco::getPredefinedDictionary(
        cv::aruco::DICT_4X4_50
    );

    cv::aruco::DetectorParameters parameters;
    cv::aruco::ArucoDetector detector(dictionary, parameters);

    std::vector<int> ids;
    std::vector<std::vector<cv::Point2f>> corners;

    detector.detectMarkers(image, corners, ids);

    std::array<cv::Point2f, 4> cameraCenters;
    std::array<bool, 4> found = {false, false, false, false};

    for (size_t i = 0; i < ids.size(); ++i) {
        int id = ids[i];

        if (id < 0 || id > 3) {
            continue;
        }

        cv::Point2f center(0.f, 0.f);

        for (const auto& point : corners[i]) {
            center += point;
        }

        center *= 0.25f;

        cameraCenters[id] = center;
        found[id] = true;
    }

    for (int id = 0; id < 4; ++id) {
        if (!found[id]) {
            std::cerr << "Marqueur ID " << id
                      << " non detecte.\n";
            return 1;
        }
    }

    std::vector<cv::Point2f> sourceCenters(
        cameraCenters.begin(), cameraCenters.end()
    );

    std::vector<cv::Point2f> rectifiedCenters;

    cv::perspectiveTransform(
        sourceCenters, rectifiedCenters, H
    );

    cv::FileStorage output(
        directory + "rgb_aruco_points.yml",
        cv::FileStorage::WRITE
    );

    if (!output.isOpened()) {
        std::cerr << "Impossible de creer le YAML.\n";
        return 1;
    }

    output << "rectified_width" << width;
    output << "rectified_height" << height;
    output << "aruco_ids" << "["
           << 0 << 1 << 2 << 3 << "]";

    output << "aruco_rectified_centers"
           << rectifiedCenters;

    output.release();

    std::cout << "Correspondances ArUco sauvegardees.\n";

    for (int id = 0; id < 4; ++id) {
        std::cout << "ID " << id
                  << " -> ("
                  << rectifiedCenters[id].x << ", "
                  << rectifiedCenters[id].y << ")\n";
    }

    return 0;
}
