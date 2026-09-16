#include <OpenNI.h>
#include <iostream>
#include <vector>
#include <string>
#include <unistd.h>

using namespace openni;
using namespace std;

struct KeyZone {
    string note;
    int xStart;
    int xEnd;
    bool isPressed;
};

int main() {

    Status rc = OpenNI::initialize();

    if (rc != STATUS_OK) {
        cerr << "Erreur OpenNI: "
             << OpenNI::getExtendedError()
             << endl;
        return 1;
    }

    Device device;
    rc = device.open(ANY_DEVICE);

    if (rc != STATUS_OK) {
        cerr << "Erreur ouverture caméra"
             << endl;
        return 1;
    }

    VideoStream depthStream;
    rc = depthStream.create(device,
                            SENSOR_DEPTH);

    if (rc != STATUS_OK) {
        cerr << "Erreur stream depth"
             << endl;
        return 1;
    }

    depthStream.start();

    VideoFrameRef frame;

    vector<KeyZone> keys = {
        {"DO", 0, 127, false},
        {"RE", 128, 255, false},
        {"MI", 256, 383, false},
        {"FA", 384, 511, false},
        {"SOL", 512, 639, false}
    };

    cout << "Bodenklavier actif."
         << endl;

    while (true) {

        rc = depthStream.readFrame(
                &frame);

        if (rc != STATUS_OK
            || !frame.isValid()) {
            continue;
        }

        const DepthPixel* depthData =
            (const DepthPixel*)
            frame.getData();

        int width =
            frame.getWidth();

        int height =
            frame.getHeight();

        int floorTop =
            height * 0.65;

        int floorBottom =
            height - 1;

        for (auto& key : keys) {

            int activePixels = 0;

            for (int y = floorTop;
                 y < floorBottom;
                 y += 5) {

                for (int x =
                     key.xStart;

                     x < key.xEnd;
                     x += 5) {

                    int index =
                        y * width + x;

                    int depth =
                        depthData[index];

                    if (depth > 200
                        && depth < 1000) {

                        activePixels++;
                    }
                }
            }

            bool detected =
                activePixels > 50;

            if (detected
                && !key.isPressed) {

                cout
                << "TOUCHE : "
                << key.note
                << endl;

                key.isPressed =
                    true;
            }

            else if (!detected) {

                key.isPressed =
                    false;
            }
        }

        usleep(50000);
    }

    depthStream.stop();
    depthStream.destroy();
    device.close();
    OpenNI::shutdown();

    return 0;
}
