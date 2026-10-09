
#include <OpenNI.h>
#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>

#include <iostream>
#include <vector>

int main() {
    using namespace openni;

    if (OpenNI::initialize() != STATUS_OK) {
        std::cerr << "Erreur initialisation OpenNI\n";
        return 1;
    }

    Device device;
    if (device.open(ANY_DEVICE) != STATUS_OK) {
        std::cerr << "Impossible d'ouvrir Astra\n";
        OpenNI::shutdown();
        return 1;
    }

    VideoStream colorStream;
    if (colorStream.create(device, SENSOR_COLOR) != STATUS_OK ||
        colorStream.start() != STATUS_OK) {
        std::cerr << "Impossible de demarrer RGB\n";
        colorStream.destroy();
        device.close();
        OpenNI::shutdown();
        return 1;
    }

    auto dictionary = cv::aruco::getPredefinedDictionary(
        cv::aruco::DICT_4X4_50
    );

    cv::aruco::ArucoDetector detector(dictionary);

    std::cout << "Detection ArUco active. Q = quitter\n";

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

        std::vector<int> ids;
        std::vector<std::vector<cv::Point2f>> corners;

        detector.detectMarkers(bgr, corners, ids);

        if (!ids.empty()) {
            cv::aruco::drawDetectedMarkers(bgr, corners, ids);
        }

        cv::putText(
            bgr,
            "Marqueurs detectes : " + std::to_string(ids.size()),
            cv::Point(15, 30),
            cv::FONT_HERSHEY_SIMPLEX,
            0.7,
            cv::Scalar(0, 255, 0),
            2
        );

        cv::imshow("Bodenklavier - ArUco Test", bgr);

        int key = cv::waitKey(1);
        if (key == 'q' || key == 'Q') {
            break;
        }
    }

    colorStream.stop();
    colorStream.destroy();
    device.close();
    OpenNI::shutdown();
    cv::destroyAllWindows();

    return 0;
}
