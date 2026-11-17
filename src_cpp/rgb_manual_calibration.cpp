
#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>

std::vector<cv::Point2f> selectedPoints;
cv::Mat displayedFrame;

void onMouse(int event, int x, int y, int, void*) {
    if (event == cv::EVENT_LBUTTONDOWN &&
        selectedPoints.size() < 4) {

        selectedPoints.emplace_back(x, y);

        cv::circle(displayedFrame,
                   cv::Point(x, y),
                   6,
                   cv::Scalar(0, 0, 255),
                   -1);

        cv::putText(displayedFrame,
                    std::to_string(selectedPoints.size() - 1),
                    cv::Point(x + 10, y),
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.7,
                    cv::Scalar(0, 0, 255),
                    2);

        cv::imshow("Calibration RGB", displayedFrame);
    }
}

int main() {
    cv::VideoCapture camera(0, cv::CAP_V4L2);

    if (!camera.isOpened()) {
        std::cerr << "Impossible d'ouvrir la webcam." << std::endl;
        return 1;
    }

    camera.set(cv::CAP_PROP_FOURCC,
               cv::VideoWriter::fourcc('M','J','P','G'));
    camera.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    camera.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    cv::Mat frame;

    if (!camera.read(frame) || frame.empty()) {
        std::cerr << "Impossible de capturer une image." << std::endl;
        return 1;
    }

    displayedFrame = frame.clone();

    cv::namedWindow("Calibration RGB");
    cv::setMouseCallback("Calibration RGB", onMouse);

    std::cout << "Clique sur les 4 coins du tapis :\n";
    std::cout << "0 : haut gauche\n";
    std::cout << "1 : haut droite\n";
    std::cout << "2 : bas droite\n";
    std::cout << "3 : bas gauche\n";
    std::cout << "S : sauvegarder, R : recommencer, Q : quitter\n";

    while (true) {
        cv::imshow("Calibration RGB", displayedFrame);

        int key = cv::waitKey(30) & 0xFF;

        if (key == 'q') {
            break;
        }

        if (key == 'r') {
            selectedPoints.clear();
            displayedFrame = frame.clone();
            std::cout << "Selection reinitialisee.\n";
        }

        if (key == 's') {
            if (selectedPoints.size() != 4) {
                std::cout << "Selectionne les 4 coins.\n";
                continue;
            }

            cv::FileStorage fs(
                "config/calibration/rgb_reference.yml",
                cv::FileStorage::WRITE
            );

            if (!fs.isOpened()) {
                std::cerr << "Erreur de sauvegarde YAML.\n";
                continue;
            }

            fs << "image_width" << frame.cols;
            fs << "image_height" << frame.rows;
            fs << "rectified_width" << 1275;
            fs << "rectified_height" << 400;
            fs << "corners" << selectedPoints;

            fs.release();

            std::cout << "Calibration RGB sauvegardee.\n";
            break;
        }
    }

    camera.release();
    cv::destroyAllWindows();
    return 0;
}
