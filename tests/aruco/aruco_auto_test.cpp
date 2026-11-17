
#include <OpenNI.h>
#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>

#include <array>
#include <cmath>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {

constexpr int OUTPUT_WIDTH = 1275;
constexpr int OUTPUT_HEIGHT = 400;

const std::string CONFIG_PATH =
    "tests/aruco/aruco_reference.yml";

struct Reference {
    int id;
    cv::Point2f local;
};

cv::Point2f markerCenter(
    const std::vector<cv::Point2f>& corners
) {
    cv::Point2f center(0, 0);
    for (const auto& p : corners) {
        center += p;
    }
    return center * 0.25f;
}

bool isFinite(const cv::Point2f& p) {
    return std::isfinite(p.x) &&
           std::isfinite(p.y);
}

} // namespace

int main() {
    using namespace openni;

    // 1. Charger la calibration enregistree
    cv::FileStorage fs(CONFIG_PATH, cv::FileStorage::READ);

    if (!fs.isOpened()) {
        std::cerr << "Impossible de lire : "
                  << CONFIG_PATH << "\n";
        return 1;
    }

    std::map<int, cv::Point2f> references;

    cv::FileNode nodes = fs["corners"];

    for (const auto& node : nodes) {
        int id = static_cast<int>(node["id"]);
        float x = static_cast<float>(
            static_cast<double>(node["local_x"])
        );
        float y = static_cast<float>(
            static_cast<double>(node["local_y"])
        );

        references[id] = cv::Point2f(x, y);
    }

    fs.release();

    if (references.size() != 4) {
        std::cerr << "Reference incomplete.\n";
        return 1;
    }

    std::cout << "Reference ArUco chargee :\n";

    for (const auto& [id, point] : references) {
        std::cout << "ID " << id
                  << " -> (" << point.x
                  << ", " << point.y << ")\n";
    }

    // 2. Initialiser Astra
    if (OpenNI::initialize() != STATUS_OK) {
        std::cerr << "Erreur OpenNI\n";
        return 1;
    }

    Device device;

    if (device.open(ANY_DEVICE) != STATUS_OK) {
        std::cerr << "Camera introuvable\n";
        OpenNI::shutdown();
        return 1;
    }

    VideoStream colorStream;

    if (colorStream.create(device, SENSOR_COLOR) != STATUS_OK ||
        colorStream.start() != STATUS_OK) {
        std::cerr << "Erreur RGB\n";
        colorStream.destroy();
        device.close();
        OpenNI::shutdown();
        return 1;
    }

    auto dictionary =
        cv::aruco::getPredefinedDictionary(
            cv::aruco::DICT_4X4_50
        );

    cv::aruco::ArucoDetector detector(dictionary);

    std::cout
        << "\nQ : quitter\n"
        << "S : sauvegarder une capture de verification\n";

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

        // Meme traitement que dans aruco_calibration.cpp
        cv::Mat flipped;
        cv::flip(bgr, flipped, 1);

        std::vector<int> ids;
        std::vector<std::vector<cv::Point2f>> corners;

        detector.detectMarkers(flipped, corners, ids);

        // Revenir aux coordonnees RGB originales
        const float width = static_cast<float>(bgr.cols);

        for (auto& marker : corners) {
            for (auto& p : marker) {
                p.x = width - 1.0f - p.x;
            }
        }

        std::map<int, std::vector<cv::Point2f>> detected;

        for (size_t i = 0; i < ids.size(); ++i) {
            if (references.count(ids[i])) {
                detected[ids[i]] = corners[i];
            }
        }

        cv::Mat display = bgr.clone();

        // Ordre physique :
        // arriere gauche, arriere droit,
        // avant droit, avant gauche
        const std::array<int, 4> orderedIds = {
            0, 1, 2, 3
        };

        std::vector<cv::Point2f> matCorners;
        bool complete = true;

        // 3. Reconstituer les coins du tapis
        for (int id : orderedIds) {
            auto it = detected.find(id);

            if (it == detected.end()) {
                complete = false;
                break;
            }

            const auto& pts = it->second;

            // Meme convention locale que lors
            // de l'enregistrement de la reference.
            std::vector<cv::Point2f> local = {
                {0, 0},
                {1, 0},
                {1, 1},
                {0, 1}
            };

            cv::Mat H = cv::getPerspectiveTransform(
                local, pts
            );

            std::vector<cv::Point2f> projected;

            cv::perspectiveTransform(
                std::vector<cv::Point2f>{references.at(id)},
                projected,
                H
            );

            if (projected.empty() ||
                !isFinite(projected[0])) {
                complete = false;
                break;
            }

            matCorners.push_back(projected[0]);

            cv::Point2f center = markerCenter(pts);

            cv::circle(
                display, center, 5,
                cv::Scalar(255, 0, 0), -1
            );

            cv::putText(
                display,
                "ID " + std::to_string(id),
                center + cv::Point2f(8, -8),
                cv::FONT_HERSHEY_SIMPLEX,
                0.6,
                cv::Scalar(255, 255, 0),
                2
            );
        }

        // 4. Dessiner le contour et rectifier
        if (complete && matCorners.size() == 4) {
            for (int i = 0; i < 4; ++i) {
                cv::line(
                    display,
                    matCorners[i],
                    matCorners[(i + 1) % 4],
                    cv::Scalar(0, 255, 0),
                    2
                );

                cv::circle(
                    display,
                    matCorners[i],
                    6,
                    cv::Scalar(0, 0, 255),
                    -1
                );

                cv::putText(
                    display,
                    "P" + std::to_string(i + 1),
                    matCorners[i] + cv::Point2f(8, 0),
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.6,
                    cv::Scalar(0, 255, 255),
                    2
                );
            }

            std::vector<cv::Point2f> target = {
                {0, 0},
                {OUTPUT_WIDTH - 1.0f, 0},
                {OUTPUT_WIDTH - 1.0f,
                 OUTPUT_HEIGHT - 1.0f},
                {0, OUTPUT_HEIGHT - 1.0f}
            };

            cv::Mat Hmat = cv::getPerspectiveTransform(
                matCorners, target
            );

            cv::Mat rectified;

            cv::warpPerspective(
                bgr,
                rectified,
                Hmat,
                cv::Size(OUTPUT_WIDTH, OUTPUT_HEIGHT)
            );

            cv::imshow("Tapis rectifie", rectified);
        }

        cv::putText(
            display,
            "ArUco: " + std::to_string(detected.size()) +
            "/4",
            cv::Point(10, 30),
            cv::FONT_HERSHEY_SIMPLEX,
            0.7,
            complete
                ? cv::Scalar(0, 255, 0)
                : cv::Scalar(0, 0, 255),
            2
        );

        cv::imshow("Calibration automatique", display);

        int key = cv::waitKey(1);

        if (key == 'q' || key == 'Q') {
            break;
        }

        if (key == 's' || key == 'S') {
            cv::imwrite(
                "tests/aruco/aruco_auto_capture.png",
                display
            );

            std::cout << "Capture enregistree.\n";
        }
    }

    colorStream.stop();
    colorStream.destroy();
    device.close();
    OpenNI::shutdown();
    cv::destroyAllWindows();

    return 0;
}
