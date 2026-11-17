
#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>
#include <iostream>
#include <vector>
#include <string>

int main() {
    const std::string yamlPath =
        "config/calibration/rgb_reference.yml";

    cv::FileStorage fs(yamlPath, cv::FileStorage::READ);

    if (!fs.isOpened()) {
        std::cerr << "Impossible de lire : " << yamlPath << "\n";
        return 1;
    }

    int imageWidth = 0, imageHeight = 0;
    int rectWidth = 0, rectHeight = 0;
    std::vector<cv::Point2f> sourcePoints;

    fs["image_width"] >> imageWidth;
    fs["image_height"] >> imageHeight;
    fs["rectified_width"] >> rectWidth;
    fs["rectified_height"] >> rectHeight;
    fs["corners"] >> sourcePoints;
    fs.release();

    if (sourcePoints.size() != 4 ||
        imageWidth <= 0 || imageHeight <= 0 ||
        rectWidth <= 0 || rectHeight <= 0) {
        std::cerr << "Calibration YAML invalide.\n";
        return 1;
    }

    std::vector<cv::Point2f> destinationPoints = {
        {0.f, 0.f},
        {float(rectWidth - 1), 0.f},
        {float(rectWidth - 1), float(rectHeight - 1)},
        {0.f, float(rectHeight - 1)}
    };

    cv::Mat homography =
        cv::getPerspectiveTransform(
            sourcePoints, destinationPoints
        );

    cv::VideoCapture camera(0, cv::CAP_V4L2);

    if (!camera.isOpened()) {
        std::cerr << "Webcam inaccessible.\n";
        return 1;
    }

    camera.set(cv::CAP_PROP_FOURCC,
        cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    camera.set(cv::CAP_PROP_FRAME_WIDTH, imageWidth);
    camera.set(cv::CAP_PROP_FRAME_HEIGHT, imageHeight);
    camera.set(cv::CAP_PROP_FPS, 30);

    auto dictionary =
        cv::aruco::getPredefinedDictionary(
            cv::aruco::DICT_4X4_50
        );

    cv::aruco::DetectorParameters parameters;
    cv::aruco::ArucoDetector detector(
        dictionary, parameters
    );

    cv::Mat frame;
    cv::Mat referenceRGB;

    std::cout << "Calibration chargee : "
              << yamlPath << "\n";
    std::cout << "C = capturer tapis vide\n";
    std::cout << "Q = quitter\n";

    while (true) {
        if (!camera.read(frame) || frame.empty()) {
            std::cerr << "Erreur de lecture video.\n";
            break;
        }

        if (frame.cols != imageWidth ||
            frame.rows != imageHeight) {
            std::cerr << "Resolution incompatible avec YAML.\n";
            break;
        }

        cv::Mat rectified;

        cv::warpPerspective(
            frame, rectified, homography,
            cv::Size(rectWidth, rectHeight)
        );

        // Detection ArUco pour visualisation.
        std::vector<int> ids;
        std::vector<std::vector<cv::Point2f>> corners;

        detector.detectMarkers(frame, corners, ids);

        cv::Mat cameraView = frame.clone();

        if (!ids.empty()) {
            cv::aruco::drawDetectedMarkers(
                cameraView, corners, ids
            );
        }

        int requiredCount = 0;
        for (int id : ids) {
            if (id >= 0 && id <= 3) {
                requiredCount++;
            }
        }

        cv::putText(
            cameraView,
            "ArUco: " + std::to_string(requiredCount) + "/4",
            cv::Point(20, 35),
            cv::FONT_HERSHEY_SIMPLEX, 0.8,
            requiredCount == 4
                ? cv::Scalar(0, 255, 0)
                : cv::Scalar(0, 0, 255),
            2
        );

        cv::imshow("RGB - Webcam", cameraView);
        // Sauvegarder une image de reference avec la touche S
       // L'image est sauvegardee sans annotations ArUco.
        cv::imshow("RGB - Tapis rectifie", rectified);

        if (!referenceRGB.empty()) {
            cv::Mat currentGray, referenceGray;
            cv::Mat difference, mask;

            cv::cvtColor(
                rectified, currentGray,
                cv::COLOR_BGR2GRAY
            );
            cv::cvtColor(
                referenceRGB, referenceGray,
                cv::COLOR_BGR2GRAY
            );

            cv::GaussianBlur(
                currentGray, currentGray,
                cv::Size(5, 5), 0
            );
            cv::GaussianBlur(
                referenceGray, referenceGray,
                cv::Size(5, 5), 0
            );

            cv::absdiff(
                currentGray, referenceGray, difference
            );

            cv::threshold(
                difference, mask, 30, 255,
                cv::THRESH_BINARY
            );

            cv::morphologyEx(
                mask, mask, cv::MORPH_OPEN,
                cv::getStructuringElement(
                    cv::MORPH_ELLIPSE,
                    cv::Size(5, 5)
                )
            );

            cv::imshow("RGB - Detection", mask);
        }

        // Une seule lecture du clavier par image.
        int key = cv::waitKey(1) & 0xFF;

        if (key == 'q') {
            break;
        }

        if (key == 'c') {
            referenceRGB = rectified.clone();
            std::cout << "Reference RGB capturee.\n";
        }

        if (key == 's') {
    bool saved = cv::imwrite(
        "config/calibration/rgb_aruco_reference.png",
        frame
    );

    if (saved) {
        std::cout << "Image de reference ArUco sauvegardee.\n";
    } else {
        std::cerr << "Erreur de sauvegarde de l'image.\n";
    }
  }
    }

    camera.release();
    cv::destroyAllWindows();

    return 0;
}
