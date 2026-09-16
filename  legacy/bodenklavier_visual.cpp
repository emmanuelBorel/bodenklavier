#include <OpenNI.h>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <unistd.h>

using namespace openni;
using namespace std;
using namespace cv;

struct KeyZone {
    string note;
    int xStart;
    int xEnd;
    bool isPressed;
};

int main() {
    OpenNI::initialize();

    Device device;
    if (device.open(ANY_DEVICE) != STATUS_OK) {
        cerr << "Erreur ouverture caméra: " << OpenNI::getExtendedError() << endl;
        return 1;
    }

    VideoStream depthStream;
    if (depthStream.create(device, SENSOR_DEPTH) != STATUS_OK) {
        cerr << "Erreur création depth stream." << endl;
        return 1;
    }

    depthStream.start();

    vector<KeyZone> keys = {
        {"DO", 0, 127, false},
        {"RE", 128, 255, false},
        {"MI", 256, 383, false},
        {"FA", 384, 511, false},
        {"SOL", 512, 639, false}
    };

    VideoFrameRef frame;

    cout << "Visualisation Bodenklavier démarrée. Appuie sur q pour quitter." << endl;

    while (true) {
        if (depthStream.readFrame(&frame) != STATUS_OK || !frame.isValid()) {
            continue;
        }

        int width = frame.getWidth();
        int height = frame.getHeight();

        const DepthPixel* depthData = (const DepthPixel*)frame.getData();

        Mat depth16(height, width, CV_16U, (void*)depthData);
        Mat depth8;
        depth16.convertTo(depth8, CV_8U, 255.0 / 4000.0);

        Mat colored;
        applyColorMap(depth8, colored, COLORMAP_JET);

        int floorTop = height * 0.65;
        int floorBottom = height - 1;

        for (auto& key : keys) {
            int activePixels = 0;

            for (int y = floorTop; y < floorBottom; y += 5) {
                for (int x = key.xStart; x < key.xEnd && x < width; x += 5) {
                    int depth = depthData[y * width + x];

                    if (depth > 200 && depth < 1000) {
                        activePixels++;
                    }
                }
            }

            bool detected = activePixels > 50;

            if (detected && !key.isPressed) {
                cout << "TOUCHE : " << key.note << endl;
                key.isPressed = true;
            } else if (!detected) {
                key.isPressed = false;
            }

            Scalar color = key.isPressed ? Scalar(0, 255, 0) : Scalar(255, 255, 255);

            rectangle(
                colored,
                Point(key.xStart, floorTop),
                Point(key.xEnd, floorBottom),
                color,
                2
            );

            putText(
                colored,
                key.note,
                Point(key.xStart + 30, floorTop + 60),
                FONT_HERSHEY_SIMPLEX,
                1.0,
                color,
                2
            );
        }

        imshow("Bodenklavier - Depth Visualisierung", colored);

        char c = (char)waitKey(1);
        if (c == 'q') {
            break;
        }

        usleep(30000);
    }

    depthStream.stop();
    depthStream.destroy();
    device.close();
    OpenNI::shutdown();

    destroyAllWindows();
    return 0;
}
