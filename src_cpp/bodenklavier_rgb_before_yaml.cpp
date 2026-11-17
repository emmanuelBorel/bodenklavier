
#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>
#include <iostream>
#include <vector>
#include <algorithm>
#include <iomanip>

int main() {
    cv::VideoCapture camera(0, cv::CAP_V4L2);
    
    

    if (!camera.isOpened()) {
        std::cerr << "Erreur : webcam inaccessible." << std::endl;
        return 1;
    }

    camera.set(cv::CAP_PROP_FOURCC,
               cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    camera.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    camera.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    camera.set(cv::CAP_PROP_FPS, 30);

    auto dictionary = cv::aruco::getPredefinedDictionary(
        cv::aruco::DICT_4X4_50
    );

    cv::aruco::DetectorParameters parameters;
    cv::aruco::ArucoDetector detector(dictionary, parameters);

    cv::Mat frame;
    cv::Mat referenceRGB;

    while (true) {
        if (!camera.read(frame) || frame.empty()) {
            std::cerr << "Erreur de lecture RGB." << std::endl;
            break;
        }

        std::vector<int> ids;
        std::vector<std::vector<cv::Point2f>> corners;

        detector.detectMarkers(frame, corners, ids);


if (ids.size() >= 4) {
    std::vector<cv::Point2f> srcPoints(4);
    std::vector<bool> found(4, false);

    for (size_t i = 0; i < ids.size(); ++i) {
        if (ids[i] >= 0 && ids[i] <= 3) {
            cv::Point2f center(0, 0);

            for (const auto& point : corners[i]) {
                center += point;
            }

            center *= 0.25f;
            srcPoints[ids[i]] = center;
            found[ids[i]] = true;
        }
    }

    if (found[0] && found[1] && found[2] && found[3]) {
        // Hypothèse à vérifier :
        // 0 = haut gauche, 1 = haut droite,
        // 2 = bas droite, 3 = bas gauche.
        std::vector<cv::Point2f> dstPoints = {
            {0, 0},
            {1274, 0},
            {1274, 399},
            {0, 399}
        };

        cv::Mat homography =
            cv::getPerspectiveTransform(srcPoints, dstPoints);

        cv::Mat rectified;

        cv::warpPerspective(
            frame,
            rectified,
            homography,
            cv::Size(1275, 400)
        );

        // Capture de la référence avec la touche C
if (cv::waitKey(1) == 'c') {
    referenceRGB = rectified.clone();
    std::cout << "Reference RGB capturee." << std::endl;
}

if (!referenceRGB.empty()) {
    cv::Mat grayCurrent, grayReference;
    cv::Mat difference, mask;

    cv::cvtColor(rectified, grayCurrent, cv::COLOR_BGR2GRAY);
    cv::cvtColor(referenceRGB, grayReference, cv::COLOR_BGR2GRAY);

    cv::GaussianBlur(grayCurrent, grayCurrent, cv::Size(5, 5), 0);
    cv::GaussianBlur(grayReference, grayReference, cv::Size(5, 5), 0);

    cv::absdiff(grayCurrent, grayReference, difference);

    cv::threshold(difference, mask, 30, 255, cv::THRESH_BINARY);

    cv::morphologyEx(
        mask, mask, cv::MORPH_OPEN,
        cv::getStructuringElement(
            cv::MORPH_ELLIPSE, cv::Size(5, 5)
        )
    );

    cv::imshow("RGB - Detection des pieds", mask);

    // Analyse provisoire des 14 zones de touches blanches.
// Les touches noires seront traitées avec PianoLayout.

const int numberOfWhiteKeys = 14;
const int keyWidth = mask.cols / numberOfWhiteKeys;

cv::Mat visualization = rectified.clone();

for (int i = 0; i < numberOfWhiteKeys; ++i) {
    int x1 = i * mask.cols / numberOfWhiteKeys;
    int x2 = (i + 1) * mask.cols / numberOfWhiteKeys;

    cv::Rect keyRegion(x1, 0, x2 - x1, mask.rows);

    int changedPixels = cv::countNonZero(mask(keyRegion));
    double occupiedRatio =
        static_cast<double>(changedPixels) / keyRegion.area();

    if (occupiedRatio > 0.10) {
        cv::rectangle(
            visualization,
            keyRegion,
            cv::Scalar(0, 0, 255),
            3
        );

        cv::putText(
            visualization,
            "ACTIVE",
            cv::Point(x1 + 5, 40),
            cv::FONT_HERSHEY_SIMPLEX,
            0.4,
            cv::Scalar(0, 0, 255),
            1
        );
    }
}

cv::imshow("RGB - Touches detectees", visualization);
}

        cv::imshow("Bodenklavier RGB - Tapis rectifie",
                   rectified);
    }
}


        if (!ids.empty()) {
            cv::aruco::drawDetectedMarkers(frame, corners, ids);
        }

        int requiredCount = 0;
        for (int id : ids) {
            if (id >= 0 && id <= 3) {
                requiredCount++;
            }
        }

        std::string status =
            "ArUco: " + std::to_string(requiredCount) + "/4";

        cv::putText(frame, status,
                    cv::Point(20, 35),
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.8,
                    requiredCount == 4
                        ? cv::Scalar(0, 255, 0)
                        : cv::Scalar(0, 0, 255),
                    2);

        cv::imshow("Bodenklavier RGB - ArUco", frame);

        if (cv::waitKey(1) == 'q') {
            break;
        }
    }

    camera.release();
    cv::destroyAllWindows();
    return 0;
}
