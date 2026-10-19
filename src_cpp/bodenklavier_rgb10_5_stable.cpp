#include <opencv2/opencv.hpp>

#include <opencv2/aruco.hpp>

#include <iostream>

#include <vector>

#include <string>

#include <array>

#include <cmath>
#include <algorithm>

#include "PianoLayout.h"
#include "ConflictResolver.h"
#include "KeyStateManager.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <map>
#include <memory>
#include <cstdint>


// Moteur audio commun avec le programme Astra Depth.
#include "AudioEngine.h"
#include "NoteMapper.h"

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
    camera.set(cv::CAP_PROP_BUFFERSIZE, 1); // Requete de buffer minimal (selon pilote V4L2)

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

    cv::Mat frame, referenceRGB, grayReferenceCached;

const cv::Rect imageBounds(0, 0, rectWidth, rectHeight);

    const auto& keys = pianoLayout.getKeys();
    ConflictResolver conflictResolver;
    KeyStateManager stateManager(keys);
    std::vector<bool> active(keys.size(), false);
    AudioEngine audio;
    NoteMapper noteMapper;
    const bool audioReady = audio.initialize("media/sounds_piano");
    if (audioReady) {
        const std::vector<std::string> notes = {
            "C4", "D4", "E4", "F4", "G4", "A4", "B4",
            "C5", "D5", "E5", "F5", "G5", "A5", "B5",
            "Cs4", "Ds4", "Fs4", "Gs4", "As4",
            "Cs5", "Ds5", "Fs5", "Gs5", "As5"
        };
        int loaded = 0;
        for (const auto& note : notes)
            if (audio.loadSound(note, note + ".wav")) ++loaded;
        std::cout << "RGB10.5 : " << loaded << "/24 echantillons audio charges.\n";
    } else {
        std::cerr << "Audio indisponible : detection maintenue.\n";
    }
    double lastDiagnosticTime = -1.0;
    int previousCandidate = -1;
    int candidateFrames = 0;
    constexpr int kSwitchConfirmationFrames = 2; // RGB10.5: moins de retard, KeyStateManager garde sa confirmation
    constexpr int kMinBlobArea = 90;

    std::cout << "RGB10.5 - RGB10.1 + audio SDL2_mixer (mono)\n"
              << "C = tapis vide | S = image ArUco | Q = quitter\n";

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

            grayReference = grayReferenceCached;

            cv::GaussianBlur(grayCurrent, grayCurrent, {5, 5}, 0);

            cv::absdiff(grayCurrent, grayReference, difference);

            cv::threshold(difference, mask, 30, 255, cv::THRESH_BINARY);

            cv::morphologyEx(mask, mask, cv::MORPH_OPEN,

                cv::getStructuringElement(cv::MORPH_ELLIPSE, {5, 5}));

            // Supprime les petites composantes isolees (bruit / taches).
            cv::Mat labels, stats, centroids;
            const int components = cv::connectedComponentsWithStats(
                mask, labels, stats, centroids, 8, CV_32S);
            cv::Mat cleaned = cv::Mat::zeros(mask.size(), CV_8UC1);
            for (int component = 1; component < components; ++component) {
                if (stats.at<int>(component, cv::CC_STAT_AREA) >= kMinBlobArea) {
                    cleaned.setTo(255, labels == component);
                }
            }
            mask = cleaned;
            cv::imshow("RGB - Detection", mask);
            // Remove black-key pixels from white-key scoring to avoid double counting.
            cv::Mat whiteMask = mask.clone();

const auto& keys = pianoLayout.getKeys();

            for (const auto& key : keys) {

                if (!key.isBlack) continue;

                cv::Rect zone = key.zone & imageBounds;

                if (zone.area() > 0) whiteMask(zone).setTo(0);

            }
            // RGB8.5 - Compare occupied ratios, not raw pixel counts.            // Black keys are smaller, so raw counts systematically favor whites.
const double minWhiteRatio = 0.05;

const double minBlackRatio = 0.08;

double bestWhiteRatio = 0.0;

double bestBlackRatio = 0.0;

const PianoKey* bestWhite = nullptr;

const PianoKey* bestBlack = nullptr;
            double black01Ratio = 0.0;
            double black02Ratio = 0.0;

            for (const auto& key : keys) {

const cv::Rect zone = key.zone & imageBounds;

                if (zone.area() <= 0) continue;

const cv::Mat& analysisMask = key.isBlack ? mask : whiteMask;

const double ratio = static_cast<double>(

                    cv::countNonZero(analysisMask(zone))) / zone.area();

                if (key.note == "BLACK_01") black01Ratio = ratio;
                if (key.note == "BLACK_02") black02Ratio = ratio;

                if (key.isBlack) {

                    if (ratio >= minBlackRatio && ratio > bestBlackRatio) {

                        bestBlackRatio = ratio;

                        bestBlack = &key;

                    }

                } else {

                    if (ratio >= minWhiteRatio && ratio > bestWhiteRatio) {

                        bestWhiteRatio = ratio;

                        bestWhite = &key;

                    }

                }

            }
            // RGB8.7: une touche noire suffisamment occupee est prioritaire.
            // Les touches blanches sont beaucoup plus larges et peuvent capter
            // des pixels du pied situe autour d'une touche noire.
            const PianoKey* dominant = nullptr;
            double dominantRatio = 0.0;
            const double blackPriorityRatio = 0.18; // 18% de la zone noire
            const double blackVsWhiteFactor = 0.60;

            if (bestBlack && bestBlackRatio >= blackPriorityRatio &&
                (!bestWhite || bestBlackRatio >= blackVsWhiteFactor * bestWhiteRatio)) {
                dominant = bestBlack;
                dominantRatio = bestBlackRatio;
            } else if (bestWhite) {
                dominant = bestWhite;
                dominantRatio = bestWhiteRatio;
            } else if (bestBlack) {
                dominant = bestBlack;
                dominantRatio = bestBlackRatio;
            }

            // RGB10: un seul candidat (baseline RGB8.7). La polyphonie
            // sera reintroduite apres validation de la detection RGB.
            std::vector<int> candidates;
            if (dominant) {
                for (size_t i = 0; i < keys.size(); ++i) {
                    if (&keys[i] == dominant) {
                        candidates.push_back(static_cast<int>(i));
                        break;
                    }
                }
            }
            // Une nouvelle touche doit etre observee plusieurs images de suite.
            // Une absence de candidat est transmise immediatement au gestionnaire
            // d'etats pour ne pas retarder RELEASE.
            int selected = candidates.empty() ? -1 : candidates.front();
            if (selected == previousCandidate) {
                candidateFrames = std::min(candidateFrames + 1, kSwitchConfirmationFrames);
            } else {
                previousCandidate = selected;
                candidateFrames = 1;
            }
            std::vector<int> stableCandidates;
            if (selected >= 0 && candidateFrames >= kSwitchConfirmationFrames)
                stableCandidates.push_back(selected);
            const auto resolved = conflictResolver.resolve(stableCandidates, keys);
            const auto events = stateManager.update(resolved);
            for (const auto& event : events) {
                if (event.keyIndex < 0 ||
                    event.keyIndex >= static_cast<int>(active.size())) continue;
                if (event.type == KeyEventType::PRESS) {
                    active[event.keyIndex] = true;
                    if (audioReady) audio.play(noteMapper.toMusicalNote(event.note));
                    std::cout << "RGB10.5 PRESS " << event.note << "\n";
                } else if (event.type == KeyEventType::RELEASE) {
                    active[event.keyIndex] = false;
                    if (audioReady) audio.stop(noteMapper.toMusicalNote(event.note));
                    std::cout << "RGB10.5 RELEASE " << event.note << "\n";
                }
            }
            cv::Mat detectionView = rectified.clone();
            for (size_t i = 0; i < keys.size(); ++i) {
                if (!active[i]) continue;
                const cv::Rect zone = keys[i].zone & imageBounds;
                if (zone.empty()) continue;
                cv::rectangle(detectionView, zone, {0, 0, 255}, 3);
                cv::putText(detectionView, keys[i].note,
                            {zone.x + 3, zone.y + 25},
                            cv::FONT_HERSHEY_SIMPLEX, 0.6, {0, 0, 255}, 2);
            }

// Diagnostic affiche une fois par seconde.
            const double now = static_cast<double>(cv::getTickCount()) /
                               cv::getTickFrequency();
            if (lastDiagnosticTime < 0 || now - lastDiagnosticTime >= 1.0) {
                std::cout << "RGB10.5 | BLACK_01=" << black01Ratio * 100.0
                          << "% | BLACK_02=" << black02Ratio * 100.0
                          << "% | meilleure blanche=" << bestWhiteRatio * 100.0
                          << "% | meilleure noire=" << bestBlackRatio * 100.0
                          << "%\n";
                lastDiagnosticTime = now;
            }

            cv::imshow("RGB - Touches detectees", detectionView);

        }

const int key = cv::waitKey(1) & 0xFF;

        if (key == 'q' || key == 'Q') break;

        if (key == 'c' || key == 'C') {

            referenceRGB = rectified.clone();
            cv::cvtColor(referenceRGB, grayReferenceCached, cv::COLOR_BGR2GRAY);
            cv::GaussianBlur(grayReferenceCached, grayReferenceCached, {5, 5}, 0);

            stateManager.reset();
            std::fill(active.begin(), active.end(), false);
            lastDiagnosticTime = -1.0;
            previousCandidate = -1;
            candidateFrames = 0;

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
