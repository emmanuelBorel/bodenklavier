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


// Sons de demonstration produits en memoire. Aucun fichier WAV externe requis.
// Les touches BLACK_XX sont associees dans l'ordre aux demi-tons classiques.
class RgbDemoAudio {
public:
    bool init(const std::vector<PianoKey>& keys) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            std::cerr << "SDL audio: " << SDL_GetError() << "\n";
            return false;
        }
        if (Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 1024) < 0) {
            std::cerr << "SDL_mixer: " << Mix_GetError() << "\n";
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            return false;
        }
        initialized_ = true;
        Mix_AllocateChannels(32);
        for (const auto& key : keys) {
            const int midi = noteToMidi(key.note);
            if (midi < 0) {
                std::cerr << "Note audio inconnue: " << key.note << "\n";
                continue;
            }
            auto pcm = makeTone(midi);
            // Mix_QuickLoad_RAW emprunte la memoire : garder les donnees vivantes.
            auto& owned = buffers_[key.note];
            owned = std::move(pcm);
            Mix_Chunk* chunk = Mix_QuickLoad_RAW(
                reinterpret_cast<Uint8*>(owned.data()),
                static_cast<Uint32>(owned.size() * sizeof(int16_t)));
            if (chunk) chunks_[key.note] = chunk;
        }
        return true;
    }
    void play(const std::string& note) {
        auto it = chunks_.find(note);
        if (it != chunks_.end()) Mix_PlayChannel(-1, it->second, 0);
    }
    ~RgbDemoAudio() {
        if (!initialized_) return;
        Mix_HaltChannel(-1);
        for (auto& item : chunks_) Mix_FreeChunk(item.second);
        Mix_CloseAudio();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
private:
    bool initialized_ = false;
    std::map<std::string, std::vector<int16_t>> buffers_;
    std::map<std::string, Mix_Chunk*> chunks_;
    static int noteToMidi(const std::string& note) {
        static const std::map<std::string,int> blacks = {
            {"BLACK_01",61},{"BLACK_02",63},{"BLACK_03",66},
            {"BLACK_04",68},{"BLACK_05",70},{"BLACK_06",73},
            {"BLACK_07",75},{"BLACK_08",78},{"BLACK_09",80},
            {"BLACK_10",82}
        };
        auto it = blacks.find(note);
        if (it != blacks.end()) return it->second;
        if (note.size() < 2) return -1;
        static const std::string names = "C D EF G A B";
        int semitone = -1;
        switch (note[0]) {
            case 'C': semitone=0; break; case 'D': semitone=2; break;
            case 'E': semitone=4; break; case 'F': semitone=5; break;
            case 'G': semitone=7; break; case 'A': semitone=9; break;
            case 'B': semitone=11; break;
        }
        if (semitone < 0 || note[1] < '0' || note[1] > '9') return -1;
        return (note[1]-'0'+1)*12+semitone;
    }
    static std::vector<int16_t> makeTone(int midi) {
        constexpr int rate=44100;
        constexpr double pi=3.14159265358979323846;
        constexpr double seconds=0.65;
        const double freq=440.0*std::pow(2.0,(midi-69)/12.0);
        const int samples=static_cast<int>(rate*seconds);
        std::vector<int16_t> result(static_cast<size_t>(samples)*2);
        for (int i=0;i<samples;++i) {
            double t=static_cast<double>(i)/rate;
            double env=(1.0-std::exp(-t*150.0))*std::exp(-t*5.5);
            double v=std::sin(2*pi*freq*t)
                    +0.30*std::sin(2*pi*freq*2*t)
                    +0.12*std::sin(2*pi*freq*3*t);
            int16_t sample=static_cast<int16_t>(std::max(-1.0,std::min(1.0,0.35*env*v))*32767);
            result[2*i]=sample;
            result[2*i+1]=sample;
        }
        return result;
    }
};

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

    const auto& keys = pianoLayout.getKeys();
    ConflictResolver conflictResolver;
    KeyStateManager stateManager(keys);
    std::vector<bool> active(keys.size(), false);
    RgbDemoAudio audio;
    if (!audio.init(keys)) std::cerr << "Audio indisponible : detection maintenue.\n";
    double lastDiagnosticTime = -1.0;
    int previousCandidate = -1;
    int candidateFrames = 0;
    constexpr int kSwitchConfirmationFrames = 4;
    constexpr int kMinBlobArea = 90;

    std::cout << "RGB10.2 - RGB10.1 + audio SDL2_mixer (mono)\n"
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

            cv::cvtColor(referenceRGB, grayReference, cv::COLOR_BGR2GRAY);

            cv::GaussianBlur(grayCurrent, grayCurrent, {5, 5}, 0);

            cv::GaussianBlur(grayReference, grayReference, {5, 5}, 0);

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
                    audio.play(event.note);
                    std::cout << "RGB10.2 PRESS " << event.note << "\n";
                } else if (event.type == KeyEventType::RELEASE) {
                    active[event.keyIndex] = false;
                    std::cout << "RGB10.2 RELEASE " << event.note << "\n";
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
                std::cout << "RGB10.2 | BLACK_01=" << black01Ratio * 100.0
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
