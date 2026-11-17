
#include <OpenNI.h>
#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>

#include <array>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {

constexpr int OUTPUT_WIDTH = 1275;
constexpr int OUTPUT_HEIGHT = 400;

// Ordre des coins dans le repère RGB original :
// arrière gauche, arrière droit, avant droit, avant gauche.
std::vector<cv::Point2f> matCorners;

// Les marqueurs sont identifiés par leur ID,
// indépendamment de leur position dans l'image.
std::map<int, std::vector<cv::Point2f>> detectedMarkers;

const std::string CONFIG_PATH = "tests/aruco/aruco_reference.yml";

void onMouse(int event, int x, int y, int, void*) {
    if (event == cv::EVENT_LBUTTONDOWN &&
        matCorners.size() < 4) {
        matCorners.emplace_back(
            static_cast<float>(x),
            static_cast<float>(y)
        );
        std::cout << "Coin " << matCorners.size()
                  << " : " << x << ", " << y << "\n";
    }
}

cv::Point2f markerCenter(
    const std::vector<cv::Point2f>& corners) {

    cv::Point2f center(0, 0);
    for (const auto& p : corners) center += p;
    return center * 0.25f;
}

} // namespace

int main() {
    using namespace openni;

    if (OpenNI::initialize() != STATUS_OK) {
        std::cerr << "Erreur OpenNI\n";
        return 1;
    }

    Device device;
    if (device.open(ANY_DEVICE) != STATUS_OK) {
        std::cerr << "Camera Astra introuvable\n";
        OpenNI::shutdown();
        return 1;
    }

    VideoStream colorStream;
    if (colorStream.create(device, SENSOR_COLOR) != STATUS_OK ||
        colorStream.start() != STATUS_OK) {
        std::cerr << "Erreur flux RGB\n";
        colorStream.destroy();
        device.close();
        OpenNI::shutdown();
        return 1;
    }

    auto dictionary = cv::aruco::getPredefinedDictionary(
        cv::aruco::DICT_4X4_50
    );
    cv::aruco::ArucoDetector detector(dictionary);

    cv::namedWindow("ArUco Calibration");
    cv::setMouseCallback("ArUco Calibration", onMouse);

    std::cout
        << "A : capturer les marqueurs\n"
        << "M : choisir 4 coins manuellement\n"
        << "S : sauvegarder la reference\n"
        << "R : effacer les coins manuels\n"
        << "Q : quitter\n";

    bool manualMode = false;

    while (true) {
        VideoFrameRef frame;
        if (colorStream.readFrame(&frame) != STATUS_OK ||
            !frame.isValid()) {
            continue;
        }

        cv::Mat rgb(
            frame.getHeight(),
            frame.getWidth(),
            CV_8UC3,
            const_cast<void*>(frame.getData()),
            frame.getStrideInBytes()
        );

        cv::Mat bgr;
        cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);

        // La camera Astra produit ici un flux dont
        // les marqueurs apparaissent en miroir.
        cv::Mat flipped;
        cv::flip(bgr, flipped, 1);

        std::vector<int> ids;
        std::vector<std::vector<cv::Point2f>> corners;

        detector.detectMarkers(flipped, corners, ids);

        // Reconvertir les coordonnees vers le RGB original.
        const float width = static_cast<float>(bgr.cols);

        for (auto& marker : corners) {
            for (auto& p : marker) {
                p.x = width - 1.0f - p.x;
            }
        }

        detectedMarkers.clear();

        for (size_t i = 0; i < ids.size(); ++i) {
            if (ids[i] >= 0 && ids[i] <= 3) {
                detectedMarkers[ids[i]] = corners[i];
            }
        }

        cv::Mat display = bgr.clone();

        for (const auto& [id, pts] : detectedMarkers) {
            cv::Point2f center = markerCenter(pts);

            cv::circle(display, center, 5,
                       cv::Scalar(0, 255, 0), -1);

            cv::putText(
                display,
                "ID " + std::to_string(id),
                center + cv::Point2f(8, -8),
                cv::FONT_HERSHEY_SIMPLEX,
                0.6,
                cv::Scalar(0, 255, 0),
                2
            );
        }

        for (size_t i = 0; i < matCorners.size(); ++i) {
            cv::circle(display, matCorners[i], 5,
                       cv::Scalar(0, 0, 255), -1);

            cv::putText(
                display,
                std::to_string(i + 1),
                matCorners[i] + cv::Point2f(8, 0),
                cv::FONT_HERSHEY_SIMPLEX,
                0.6,
                cv::Scalar(0, 0, 255),
                2
            );
        }

        cv::putText(
            display,
            "Markers: " + std::to_string(detectedMarkers.size()) +
            "/4  Corners: " + std::to_string(matCorners.size()) +
            "/4",
            cv::Point(10, 30),
            cv::FONT_HERSHEY_SIMPLEX,
            0.6,
            cv::Scalar(0, 255, 255),
            2
        );

        cv::imshow("ArUco Calibration", display);

        int key = cv::waitKey(1);

        if (key == 'q' || key == 'Q') break;

        if (key == 'm' || key == 'M') {
            manualMode = true;
            matCorners.clear();
            std::cout << "Clique les 4 coins du tapis.\n";
        }

        if (key == 'r' || key == 'R') {
            matCorners.clear();
        }

        if (key == 'a' || key == 'A') {
            std::cout << "Marqueurs visibles : "
                      << detectedMarkers.size() << "/4\n";
        }

        if (key == 's' || key == 'S') {
            if (!manualMode ||
                matCorners.size() != 4 ||
                detectedMarkers.size() != 4) {

                std::cout
                    << "Reference incomplete : 4 marqueurs "
                    << "et 4 coins necessaires.\n";
                continue;
            }

            // Chaque coin du tapis est memorise dans
            // le repere local de son marqueur.
            // La relation est une homographie locale.
            cv::FileStorage fs(
                CONFIG_PATH,
                cv::FileStorage::WRITE
            );

            fs << "output_width" << OUTPUT_WIDTH;
            fs << "output_height" << OUTPUT_HEIGHT;
            fs << "marker_size_cm" << 10.5;

            // Association a verifier sur l'installation.
            const std::array<int, 4> markerIds = {
                0, 1, 2, 3
            };

            fs << "corners" << "[";

            for (size_t i = 0; i < 4; ++i) {
                const int id = markerIds[i];

                const auto& pts = detectedMarkers.at(id);

                std::vector<cv::Point2f> local = {
                    {0, 0},
                    {1, 0},
                    {1, 1},
                    {0, 1}
                };

                // Les coins ArUco renvoyes apres flip
                // ont un ordre miroir : on sauvegarde
                // la transformation correspondante.
                cv::Mat H = cv::getPerspectiveTransform(
                    pts, local
                );

                std::vector<cv::Point2f> projected;
                cv::perspectiveTransform(
                    std::vector<cv::Point2f>{matCorners[i]},
                    projected,
                    H
                );

                fs << "{:"
                   << "id" << id
                   << "local_x" << projected[0].x
                   << "local_y" << projected[0].y
                   << "}";
            }

            fs << "]";
            fs.release();

            std::cout
                << "Reference enregistree : "
                << CONFIG_PATH << "\n";
        }
    }

    colorStream.stop();
    colorStream.destroy();
    device.close();
    OpenNI::shutdown();
    cv::destroyAllWindows();

    return 0;
}
