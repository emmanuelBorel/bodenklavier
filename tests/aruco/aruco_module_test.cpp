
#include "ArucoCalibration.h"

#include <iostream>

int main() {
    ArucoCalibration calibration(
        "tests/aruco/aruco_reference.yml",
        30
    );

    if (!calibration.loadReference()) {
        return 1;
    }

    std::cout
        << "Module ArUco initialise correctement.\n"
        << "Images requises : "
        << calibration.getRequiredFrames()
        << "\n";

    return 0;
}
