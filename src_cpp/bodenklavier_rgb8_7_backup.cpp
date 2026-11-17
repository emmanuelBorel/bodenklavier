#include <opencv2/opencv.hpp>

#include <opencv2/aruco.hpp>

#include <iostream>

#include <vector>

#include <string>

#include <array>

#include <cmath>
#include <algorithm>
#include <sstream>

#include "PianoLayout.h"

int main() {

const std::string calibrationDir = "config/calibration/";

    cv::FileStorage fs(calibrationDir + "rgb_reference.yml", cv::FileStorage::READ);

    if (!fs.isOpened()) {

        std::cerr << "Impossible de lire rgb_reference.yml\n";

        return 1;

    }

int imageWidth = 0, imageHeight = 0, rectWidth = 0, rectHeight = 0;

    std::vector<cv::Point2f> matCorners;

    fs["image_width"] >> imageWidth;

    fs["image_height"] >> imageHeight;

    fs["rectified_width"] >> rectWidth;

    fs["rectified_height"] >> rectHeight;

    fs["corners"] >> matCorners;

    fs.release();

    if (matCorners.size() != 4 || imageWidth <= 0 || imageHeight <= 0 ||

        rectWidth <= 0 || rectHeight <= 0) {

        std::cerr << "Calibration RGB invalide.\n";

        return 1;

    }

const std::vector<cv::Point2f> rectCorners = {

        {0.f, 0.f}, {float(rectWidth - 1), 0.f},

        {float(rectWidth - 1), float(rectHeight - 1)},

        {0.f, float(rectHeight - 1)}

    };

    cv::Mat homography = cv::getPerspectiveTransform(matCorners, rectCorners);

    cv::FileStorage arucoFile(calibrationDir + "rgb_aruco_points.yml", cv::FileStorage::READ);

    if (!arucoFile.isOpened()) {

        std::cerr << "Impossible de lire rgb_aruco_points.yml\n";

        return 1;

    }

    std::vector<cv::Point2f> arucoReference;

    arucoFile["aruco_rectified_centers"] >> arucoReference;

    arucoFile.release();

    if (arucoReference.size() != 4) {

        std::cerr << "Reference ArUco invalide (4 centres requis).\n";

        return 1;

    }

    cv::VideoCapture camera(0, cv::CAP_V4L2);

    if (!camera.isOpened()) {

        std::cerr << "Webcam inaccessible.\n";

        return 1;

    }

    camera.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));

    camera.set(cv::CAP_PROP_FRAME_WIDTH, imageWidth);

    camera.set(cv::CAP_PROP_FRAME_HEIGHT, imageHeight);

    camera.set(cv::CAP_PROP_FPS, 30);

auto dictionary = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_4X4_50);

    cv::aruco::DetectorParameters parameters;
    parameters.adaptiveThreshWinSizeMin = 3;
    parameters.adaptiveThreshWinSizeMax = 53;
    parameters.adaptiveThreshWinSizeStep = 4;
    parameters.minMarkerPerimeterRate = 0.015;
    parameters.cornerRefinementMethod = cv::aruco::CORNER_REFINE_SUBPIX;
    parameters.errorCorrectionRate = 0.6;

    cv::aruco::ArucoDetector detector(dictionary, parameters);

    PianoLayout pianoLayout(rectWidth, rectHeight);

    cv::Mat frame, referenceRGB;

const cv::Rect imageBounds(0, 0, rectWidth, rectHeight);

    // RGB9: une machine d'etat par touche (polyphonie visuelle).
    const auto& keys = pianoLayout.getKeys();
    struct KeyState { int onFrames = 0; int offFrames = 0; bool active = false; };
    std::vector<KeyState> states(keys.size());
    const int confirmFrames = 3;
    const int releaseFrames = 4;
    const double minBlackRatio = 0.08;
    const double minWhiteRatio = 0.05;
    const double blackPriorityRatio = 0.18;
    double lastDiagnosticTime = -1.0;
    std::cout << "RGB9 - Detection polyphonique experimentale (sans audio)\n"
              << "C = capturer le tapis vide | S = image ArUco | Q = quitter\n";

    while (true) {

        if (!camera.read(frame) || frame.empty()) {

            std::cerr << "Erreur de lecture video.\n";

            break;

        }

        if (frame.cols != imageWidth || frame.rows != imageHeight) {

            std::cerr << "Resolution incompatible avec la calibration.\n";

            break;

        }

        std::vector<int> ids;

        std::vector<std::vector<cv::Point2f>> corners;

        detector.detectMarkers(frame, corners, ids);

        std::array<cv::Point2f, 4> detectedCenters{};

        std::array<bool, 4> found{};

        for (size_t i = 0; i < ids.size(); ++i) {

const int id = ids[i];

            if (id < 0 || id > 3 || corners[i].size() != 4) continue;

            cv::Point2f center(0.f, 0.f);

            for (const auto& p : corners[i]) center += p;

            detectedCenters[id] = center * 0.25f;

            found[id] = true;

        }

const bool allFound = found[0] && found[1] && found[2] && found[3];

        if (allFound) {

            std::vector<cv::Point2f> src(detectedCenters.begin(), detectedCenters.end());

            cv::Mat candidate = cv::getPerspectiveTransform(src, arucoReference);

            if (cv::checkRange(candidate) && std::abs(cv::determinant(candidate)) > 1e-12) {

                homography = candidate;

            }

        }

        cv::Mat rectified;

        cv::warpPerspective(frame, rectified, homography,

                            cv::Size(rectWidth, rectHeight));

        cv::Mat cameraView = frame.clone();

        if (!ids.empty()) cv::aruco::drawDetectedMarkers(cameraView, corners, ids);

int requiredCount = 0;

        for (bool present : found) if (present) ++requiredCount;

        cv::putText(cameraView, "ArUco: " + std::to_string(requiredCount) + "/4",

                    {20, 35}, cv::FONT_HERSHEY_SIMPLEX, 0.8,

                    allFound ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255), 2);

        cv::putText(cameraView,

                    allFound ? "AUTO CALIBRATION: OK" : "AUTO CALIBRATION: LAST VALID",

                    {20, 70}, cv::FONT_HERSHEY_SIMPLEX, 0.55,

                    allFound ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 165, 255), 2);

        cv::imshow("RGB - Webcam", cameraView);

        cv::Mat layoutView = rectified.clone();

        pianoLayout.draw(layoutView);

        cv::imshow("RGB - Tapis rectifie", layoutView);

        if (!referenceRGB.empty()) {

            cv::Mat grayCurrent, grayReference, difference, mask;

            cv::cvtColor(rectified, grayCurrent, cv::COLOR_BGR2GRAY);

            cv::cvtColor(referenceRGB, grayReference, cv::COLOR_BGR2GRAY);

            cv::GaussianBlur(grayCurrent, grayCurrent, {5, 5}, 0);

            cv::GaussianBlur(grayReference, grayReference, {5, 5}, 0);

            cv::absdiff(grayCurrent, grayReference, difference);

            cv::threshold(difference, mask, 30, 255, cv::THRESH_BINARY);

            cv::morphologyEx(mask, mask, cv::MORPH_OPEN,

                cv::getStructuringElement(cv::MORPH_ELLIPSE, {5, 5}));

            cv::imshow("RGB - Detection", mask);
            // Soustraire les zones noires pour eviter leur double comptage
            // dans les zones blanches qui les recouvrent.
            cv::Mat whiteMask = mask.clone();
            for (const auto& k : keys) {
                if (!k.isBlack) continue;
                cv::Rect zone = k.zone & imageBounds;
                if (zone.area() > 0) whiteMask(zone).setTo(0);
            }

            // Un objet connecte correspond a un candidat de contact.
            // Chaque composante est attribuee a AU PLUS une touche, ce qui
            // limite les faux positifs sur les touches voisines.
            cv::Mat labels, stats, centroids;
            int componentCount = cv::connectedComponentsWithStats(
                mask, labels, stats, centroids, 8, CV_32S);
            std::vector<bool> observed(keys.size(), false);
            const int minComponentArea = 120; // pixels dans la vue rectifiee

            for (int component = 1; component < componentCount; ++component) {
                const int area = stats.at<int>(component, cv::CC_STAT_AREA);
                if (area < minComponentArea) continue;
                cv::Rect componentBox(
                    stats.at<int>(component, cv::CC_STAT_LEFT),
                    stats.at<int>(component, cv::CC_STAT_TOP),
                    stats.at<int>(component, cv::CC_STAT_WIDTH),
                    stats.at<int>(component, cv::CC_STAT_HEIGHT));
                componentBox &= imageBounds;
                if (componentBox.empty()) continue;

                cv::Mat componentMask = (labels(componentBox) == component);
                int bestIndex = -1;
                double bestScore = 0.0;
                for (size_t i = 0; i < keys.size(); ++i) {
                    const auto& k = keys[i];
                    cv::Rect zone = k.zone & componentBox;
                    if (zone.empty()) continue;
                    cv::Rect fullZone = k.zone & imageBounds;
                    if (fullZone.empty()) continue;
                    const cv::Rect localZone(zone.x - componentBox.x,
                                             zone.y - componentBox.y,
                                             zone.width, zone.height);
                    cv::Mat overlap;
                    cv::bitwise_and(componentMask(localZone),
                                    (k.isBlack ? mask : whiteMask)(zone), overlap);
                    const int pixels = cv::countNonZero(overlap);
                    const double zoneRatio = double(pixels) / fullZone.area();
                    const double componentShare = double(pixels) / area;
                    if (zoneRatio < (k.isBlack ? minBlackRatio : minWhiteRatio) ||
                        componentShare < 0.12) continue;

                    // Priorite aux touches noires si elles sont suffisamment
                    // couvertes, comme dans RGB8.7.
                    double score = zoneRatio * (0.5 + componentShare);
                    if (k.isBlack && zoneRatio >= blackPriorityRatio)
                        score *= 1.7;
                    if (score > bestScore) {
                        bestScore = score;
                        bestIndex = static_cast<int>(i);
                    }
                }
                if (bestIndex >= 0) observed[bestIndex] = true;
            }

            cv::Mat detectionView = rectified.clone();
            std::vector<std::string> activeNotes;
            for (size_t i = 0; i < keys.size(); ++i) {
                auto& state = states[i];
                if (observed[i]) {
                    state.offFrames = 0;
                    state.onFrames = std::min(state.onFrames + 1, confirmFrames);
                    if (!state.active && state.onFrames >= confirmFrames) {
                        state.active = true;
                        std::cout << "RGB9 ON  " << keys[i].note << "\n";
                    }
                } else {
                    state.onFrames = 0;
                    state.offFrames = std::min(state.offFrames + 1, releaseFrames);
                    if (state.active && state.offFrames >= releaseFrames) {
                        state.active = false;
                        std::cout << "RGB9 OFF " << keys[i].note << "\n";
                    }
                }
                if (!state.active) continue;
                activeNotes.push_back(keys[i].note);
                cv::Rect zone = keys[i].zone & imageBounds;
                if (zone.empty()) continue;
                cv::rectangle(detectionView, zone, cv::Scalar(0, 0, 255), 3);
                cv::putText(detectionView, keys[i].note,
                            {zone.x + 3, zone.y + 25},
                            cv::FONT_HERSHEY_SIMPLEX, 0.55,
                            cv::Scalar(0, 0, 255), 2);
            }
            const double now = double(cv::getTickCount()) / cv::getTickFrequency();
            if (lastDiagnosticTime < 0 || now - lastDiagnosticTime >= 1.0) {
                std::ostringstream oss;
                for (size_t i = 0; i < activeNotes.size(); ++i) {
                    if (i) oss << " + ";
                    oss << activeNotes[i];
                }
                std::cout << "RGB9 actives (" << activeNotes.size() << "): "
                          << (activeNotes.empty() ? "aucune" : oss.str()) << "\n";
                lastDiagnosticTime = now;
            }
            cv::imshow("RGB - Touches detectees", detectionView);

        }

const int key = cv::waitKey(1) & 0xFF;

        if (key == 'q' || key == 'Q') break;

        if (key == 'c' || key == 'C') {

            referenceRGB = rectified.clone();

            for (auto& state : states) state = KeyState{};
            lastDiagnosticTime = -1.0;
            std::cout << "Reference RGB capturee.\n";

        }

        if (key == 's' || key == 'S') {

const bool saved = cv::imwrite(calibrationDir + "rgb_aruco_reference.png", frame);

            std::cout << (saved ? "Image de reference ArUco sauvegardee.\n"

                                : "Erreur de sauvegarde de l'image.\n");

        }

    }

    camera.release();

    cv::destroyAllWindows();

    return 0;

}
