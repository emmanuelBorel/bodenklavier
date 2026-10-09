#include <OpenNI.h>
#include <opencv2/opencv.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

using namespace openni;
using namespace cv;
using namespace std;

namespace
{
    const string RGB_WINDOW   = "Bodenkontakt - RGB";
    const string DEPTH_WINDOW = "Bodenkontakt - Depth";

    constexpr int ROI_RADIUS = 25;
    constexpr int NUMBER_OF_FRAMES = 30;

    Point selectedColorPoint(-1, -1);
    bool pointSelected = false;

    struct Measurement
    {
        string state;

        int validPixels = 0;
        int changedPixels = 0;

        double changedRatio = 0.0;

        double meanDifference = 0.0;
        double medianDifference = 0.0;
        double minDifference = 0.0;
        double maxDifference = 0.0;
    };


    // ========================================================
    // SOURIS
    // ========================================================

    void mouseCallback(
        int event,
        int x,
        int y,
        int,
        void*
    )
    {
        if (event == EVENT_LBUTTONDOWN)
        {
            selectedColorPoint = Point(x, y);
            pointSelected = true;

            cout
                << "Point RGB selectionne : "
                << x << ", " << y
                << endl;
        }
    }


    // ========================================================
    // ROI
    // ========================================================

    Rect createROI(
        const Point& center,
        int width,
        int height
    )
    {
        const int x1 =
            max(0, center.x - ROI_RADIUS);

        const int y1 =
            max(0, center.y - ROI_RADIUS);

        const int x2 =
            min(width, center.x + ROI_RADIUS + 1);

        const int y2 =
            min(height, center.y + ROI_RADIUS + 1);

        return Rect(
            x1,
            y1,
            x2 - x1,
            y2 - y1
        );
    }


    // ========================================================
    // LECTURE DEPTH
    // ========================================================

    bool readDepthFrame(
        VideoStream& stream,
        Mat& depth
    )
    {
        VideoFrameRef frame;

        if (
            stream.readFrame(&frame) != STATUS_OK
            ||
            !frame.isValid()
        )
        {
            return false;
        }

        const int width =
            frame.getWidth();

        const int height =
            frame.getHeight();

        const DepthPixel* data =
            static_cast<const DepthPixel*>(
                frame.getData()
            );

        Mat temporary(
            height,
            width,
            CV_16U,
            const_cast<DepthPixel*>(data)
        );

        depth = temporary.clone();

        return true;
    }


    // ========================================================
    // LECTURE RGB
    // ========================================================

    bool readColorFrame(
        VideoStream& stream,
        Mat& bgr
    )
    {
        VideoFrameRef frame;

        if (
            stream.readFrame(&frame) != STATUS_OK
            ||
            !frame.isValid()
        )
        {
            return false;
        }

        const int width =
            frame.getWidth();

        const int height =
            frame.getHeight();

        const RGB888Pixel* data =
            static_cast<const RGB888Pixel*>(
                frame.getData()
            );

        Mat rgb(
            height,
            width,
            CV_8UC3,
            const_cast<RGB888Pixel*>(data)
        );

        cvtColor(
            rgb,
            bgr,
            COLOR_RGB2BGR
        );

        return true;
    }


    // ========================================================
    // MOYENNE SUR PLUSIEURS FRAMES
    // ========================================================

    Mat captureAverageDepth(
        VideoStream& depthStream
    )
    {
        Mat firstFrame;

        while (
            !readDepthFrame(
                depthStream,
                firstFrame
            )
        )
        {
        }

        Mat sum =
            Mat::zeros(
                firstFrame.size(),
                CV_64F
            );

        Mat count =
            Mat::zeros(
                firstFrame.size(),
                CV_32S
            );


        for (
            int frameIndex = 0;
            frameIndex < NUMBER_OF_FRAMES;
            ++frameIndex
        )
        {
            Mat depth;

            if (
                !readDepthFrame(
                    depthStream,
                    depth
                )
            )
            {
                --frameIndex;
                continue;
            }


            for (
                int y = 0;
                y < depth.rows;
                ++y
            )
            {
                const uint16_t* depthRow =
                    depth.ptr<uint16_t>(y);

                double* sumRow =
                    sum.ptr<double>(y);

                int* countRow =
                    count.ptr<int>(y);


                for (
                    int x = 0;
                    x < depth.cols;
                    ++x
                )
                {
                    const uint16_t value =
                        depthRow[x];

                    if (value == 0)
                    {
                        continue;
                    }

                    sumRow[x] += value;
                    countRow[x]++;
                }
            }
        }


        Mat average =
            Mat::zeros(
                firstFrame.size(),
                CV_16U
            );


        for (
            int y = 0;
            y < average.rows;
            ++y
        )
        {
            uint16_t* outputRow =
                average.ptr<uint16_t>(y);

            const double* sumRow =
                sum.ptr<double>(y);

            const int* countRow =
                count.ptr<int>(y);


            for (
                int x = 0;
                x < average.cols;
                ++x
            )
            {
                if (countRow[x] > 0)
                {
                    outputRow[x] =
                        static_cast<uint16_t>(
                            lround(
                                sumRow[x]
                                /
                                countRow[x]
                            )
                        );
                }
            }
        }

        return average;
    }


    // ========================================================
    // CALCUL MESURE
    // ========================================================

    Measurement calculateMeasurement(
        const string& state,
        const Mat& referenceDepth,
        const Mat& currentDepth,
        const Rect& roi
    )
    {
        Measurement result;

        result.state = state;

        vector<double> differences;


        for (
            int y = roi.y;
            y < roi.y + roi.height;
            ++y
        )
        {
            const uint16_t* referenceRow =
                referenceDepth.ptr<uint16_t>(y);

            const uint16_t* currentRow =
                currentDepth.ptr<uint16_t>(y);


            for (
                int x = roi.x;
                x < roi.x + roi.width;
                ++x
            )
            {
                const uint16_t referenceValue =
                    referenceRow[x];

                const uint16_t currentValue =
                    currentRow[x];


                if (
                    referenceValue == 0
                    ||
                    currentValue == 0
                )
                {
                    continue;
                }


                result.validPixels++;


                const double difference =
                    static_cast<double>(
                        referenceValue
                    )
                    -
                    static_cast<double>(
                        currentValue
                    );


                if (difference > 0.0)
                {
                    differences.push_back(
                        difference
                    );
                }


                // Seulement pour quantifier les pixels
                // significativement differents.
                // Ce n'est PAS le seuil final de contact.
                if (difference > 30.0)
                {
                    result.changedPixels++;
                }
            }
        }


        if (result.validPixels > 0)
        {
            result.changedRatio =
                static_cast<double>(
                    result.changedPixels
                )
                /
                result.validPixels;
        }


        if (!differences.empty())
        {
            sort(
                differences.begin(),
                differences.end()
            );


            result.minDifference =
                differences.front();

            result.maxDifference =
                differences.back();


            result.meanDifference =
                accumulate(
                    differences.begin(),
                    differences.end(),
                    0.0
                )
                /
                differences.size();


            const size_t n =
                differences.size();


            if (n % 2 == 0)
            {
                result.medianDifference =
                    (
                        differences[n / 2 - 1]
                        +
                        differences[n / 2]
                    )
                    / 2.0;
            }
            else
            {
                result.medianDifference =
                    differences[n / 2];
            }
        }


        return result;
    }


    // ========================================================
    // AFFICHAGE MESURE
    // ========================================================

    void printMeasurement(
        const Measurement& m
    )
    {
        cout
            << "\n====================================\n"
            << "MESURE : " << m.state << "\n"
            << "====================================\n"
            << "Pixels valides      : "
            << m.validPixels << "\n"
            << "Pixels modifies     : "
            << m.changedPixels << "\n"
            << "Ratio modifie       : "
            << fixed << setprecision(3)
            << m.changedRatio * 100.0
            << " %\n"
            << "Difference moyenne  : "
            << m.meanDifference
            << " mm\n"
            << "Difference mediane  : "
            << m.medianDifference
            << " mm\n"
            << "Difference minimale : "
            << m.minDifference
            << " mm\n"
            << "Difference maximale : "
            << m.maxDifference
            << " mm\n"
            << "====================================\n"
            << endl;
    }


    // ========================================================
    // CSV
    // ========================================================

    void saveMeasurement(
        const string& csvPath,
        const Measurement& m
    )
    {
        const bool exists =
            filesystem::exists(
                csvPath
            );

        ofstream file(
            csvPath,
            ios::app
        );


        if (!file)
        {
            cerr
                << "Impossible d'ouvrir "
                << csvPath
                << endl;

            return;
        }


        if (!exists)
        {
            file
                << "state,"
                << "valid_pixels,"
                << "changed_pixels,"
                << "changed_ratio,"
                << "mean_difference_mm,"
                << "median_difference_mm,"
                << "min_difference_mm,"
                << "max_difference_mm\n";
        }


        file
            << m.state << ","
            << m.validPixels << ","
            << m.changedPixels << ","
            << m.changedRatio << ","
            << m.meanDifference << ","
            << m.medianDifference << ","
            << m.minDifference << ","
            << m.maxDifference
            << "\n";
    }
}


int main()
{
    cout
        << "\n========================================\n"
        << " BODENKONTAKT - EXPERIMENT ASTRA\n"
        << " RGB + DEPTH\n"
        << "========================================\n"
        << endl;


    // ========================================================
    // OPENNI
    // ========================================================

    if (
        OpenNI::initialize()
        != STATUS_OK
    )
    {
        cerr
            << "Erreur OpenNI : "
            << OpenNI::getExtendedError()
            << endl;

        return 1;
    }


    Device device;


    if (
        device.open(ANY_DEVICE)
        != STATUS_OK
    )
    {
        cerr
            << "Erreur camera : "
            << OpenNI::getExtendedError()
            << endl;

        OpenNI::shutdown();

        return 1;
    }


    // ========================================================
    // DEPTH
    // ========================================================

    VideoStream depthStream;


    if (
        depthStream.create(
            device,
            SENSOR_DEPTH
        )
        != STATUS_OK
    )
    {
        cerr
            << "Erreur creation Depth."
            << endl;

        device.close();
        OpenNI::shutdown();

        return 1;
    }


    if (
        depthStream.start()
        != STATUS_OK
    )
    {
        cerr
            << "Erreur demarrage Depth."
            << endl;

        depthStream.destroy();
        device.close();
        OpenNI::shutdown();

        return 1;
    }


    cout
        << "Flux Depth demarre."
        << endl;


    // ========================================================
    // RGB
    // ========================================================

    VideoStream colorStream;


    if (
        !device.hasSensor(
            SENSOR_COLOR
        )
    )
    {
        cerr
            << "Aucun capteur couleur."
            << endl;

        depthStream.stop();
        depthStream.destroy();
        device.close();
        OpenNI::shutdown();

        return 1;
    }


    if (
        colorStream.create(
            device,
            SENSOR_COLOR
        )
        != STATUS_OK
    )
    {
        cerr
            << "Erreur creation RGB."
            << endl;

        depthStream.stop();
        depthStream.destroy();
        device.close();
        OpenNI::shutdown();

        return 1;
    }


    if (
        colorStream.start()
        != STATUS_OK
    )
    {
        cerr
            << "Erreur demarrage RGB."
            << endl;

        colorStream.destroy();

        depthStream.stop();
        depthStream.destroy();

        device.close();
        OpenNI::shutdown();

        return 1;
    }


    cout
        << "Flux RGB demarre."
        << endl;


    // ========================================================
    // REGISTRATION DEPTH -> RGB
    // ========================================================

    bool registrationEnabled = false;


    if (
        device.isImageRegistrationModeSupported(
            IMAGE_REGISTRATION_DEPTH_TO_COLOR
        )
    )
    {
        const Status rc =
            device.setImageRegistrationMode(
                IMAGE_REGISTRATION_DEPTH_TO_COLOR
            );


        if (rc == STATUS_OK)
        {
            registrationEnabled = true;

            cout
                << "Registration DEPTH_TO_COLOR activee."
                << endl;
        }
        else
        {
            cerr
                << "Echec registration : "
                << OpenNI::getExtendedError()
                << endl;
        }
    }
    else
    {
        cerr
            << "Registration DEPTH_TO_COLOR "
            << "non supportee."
            << endl;
    }


    // ========================================================
    // STABILISATION
    // ========================================================

    for (int i = 0; i < 30; ++i)
    {
        Mat ignored;

        readDepthFrame(
            depthStream,
            ignored
        );
    }


    // ========================================================
    // FENETRES
    // ========================================================

    namedWindow(
        RGB_WINDOW,
        WINDOW_NORMAL
    );

    namedWindow(
        DEPTH_WINDOW,
        WINDOW_NORMAL
    );


    setMouseCallback(
        RGB_WINDOW,
        mouseCallback
    );


    // ========================================================
    // VARIABLES
    // ========================================================

    Mat currentDepth;
    Mat currentColor;
    Mat referenceDepth;

    bool referenceCaptured = false;


    filesystem::create_directories(
        "evaluation/bodenkontakt"
    );


    const string csvPath =
        "evaluation/bodenkontakt/"
        "bodenkontakt_measurements.csv";


    cout
        << "\nCOMMANDES\n"
        << "----------------------------------------\n"
        << "Clic gauche RGB : choisir une touche\n"
        << "R : reference tapis vide\n"
        << "1 : EMPTY\n"
        << "2 : HOVER_20\n"
        << "3 : HOVER_10\n"
        << "4 : HOVER_5\n"
        << "5 : CONTACT\n"
        << "Q : quitter\n"
        << "----------------------------------------\n"
        << endl;


    // ========================================================
    // BOUCLE
    // ========================================================

    while (true)
    {
        if (
            !readDepthFrame(
                depthStream,
                currentDepth
            )
        )
        {
            continue;
        }


        if (
            !readColorFrame(
                colorStream,
                currentColor
            )
        )
        {
            continue;
        }


        // ----------------------------------------------------
        // Depth visualization
        // ----------------------------------------------------

        Mat depth8;

        currentDepth.convertTo(
            depth8,
            CV_8U,
            255.0 / 4000.0
        );


        Mat depthDisplay;

        applyColorMap(
            depth8,
            depthDisplay,
            COLORMAP_JET
        );


        Mat colorDisplay =
            currentColor.clone();


        // ----------------------------------------------------
        // Selected ROI
        // ----------------------------------------------------

        if (pointSelected)
        {
            // Coordonnees RGB -> Depth.
            //
            // Meme avec registration activee, on ne suppose
            // pas que les deux resolutions sont identiques.

            const double scaleX =
                static_cast<double>(
                    currentDepth.cols
                )
                /
                static_cast<double>(
                    currentColor.cols
                );


            const double scaleY =
                static_cast<double>(
                    currentDepth.rows
                )
                /
                static_cast<double>(
                    currentColor.rows
                );


            Point depthPoint(
                static_cast<int>(
                    lround(
                        selectedColorPoint.x
                        *
                        scaleX
                    )
                ),

                static_cast<int>(
                    lround(
                        selectedColorPoint.y
                        *
                        scaleY
                    )
                )
            );


            depthPoint.x =
                clamp(
                    depthPoint.x,
                    0,
                    currentDepth.cols - 1
                );

            depthPoint.y =
                clamp(
                    depthPoint.y,
                    0,
                    currentDepth.rows - 1
                );


            Rect colorROI =
                createROI(
                    selectedColorPoint,
                    currentColor.cols,
                    currentColor.rows
                );


            Rect depthROI =
                createROI(
                    depthPoint,
                    currentDepth.cols,
                    currentDepth.rows
                );


            rectangle(
                colorDisplay,
                colorROI,
                Scalar(0, 255, 0),
                2
            );


            circle(
                colorDisplay,
                selectedColorPoint,
                4,
                Scalar(0, 255, 0),
                FILLED
            );


            rectangle(
                depthDisplay,
                depthROI,
                Scalar(255, 255, 255),
                2
            );


            circle(
                depthDisplay,
                depthPoint,
                4,
                Scalar(255, 255, 255),
                FILLED
            );
        }


        putText(
            colorDisplay,

            registrationEnabled
                ? "Registration: ON"
                : "Registration: OFF",

            Point(20, 35),

            FONT_HERSHEY_SIMPLEX,
            0.8,
            Scalar(0, 255, 0),
            2
        );


        putText(
            depthDisplay,

            referenceCaptured
                ? "Reference: OK"
                : "Reference: NON",

            Point(20, 35),

            FONT_HERSHEY_SIMPLEX,
            0.8,
            Scalar(255, 255, 255),
            2
        );


        imshow(
            RGB_WINDOW,
            colorDisplay
        );


        imshow(
            DEPTH_WINDOW,
            depthDisplay
        );


        const int key =
            waitKey(1) & 0xFF;


        // ----------------------------------------------------
        // Quit
        // ----------------------------------------------------

        if (
            key == 'q'
            ||
            key == 'Q'
            ||
            key == 27
        )
        {
            break;
        }


        // ----------------------------------------------------
        // Reference
        // ----------------------------------------------------

        if (
            key == 'r'
            ||
            key == 'R'
        )
        {
            cout
                << "\nCapture reference..."
                << endl;

            cout
                << "Le tapis doit etre vide."
                << endl;


            referenceDepth =
                captureAverageDepth(
                    depthStream
                );


            referenceCaptured = true;


            cout
                << "Reference enregistree sur "
                << NUMBER_OF_FRAMES
                << " frames."
                << endl;

            continue;
        }


        // ----------------------------------------------------
        // Etat
        // ----------------------------------------------------

        string state;


        if (key == '1')
        {
            state = "EMPTY";
        }
        else if (key == '2')
        {
            state = "HOVER_20";
        }
        else if (key == '3')
        {
            state = "HOVER_10";
        }
        else if (key == '4')
        {
            state = "HOVER_5";
        }
        else if (key == '5')
        {
            state = "CONTACT";
        }
        else
        {
            continue;
        }


        if (!pointSelected)
        {
            cout
                << "Clique d'abord sur une touche "
                << "dans la fenetre RGB."
                << endl;

            continue;
        }


        if (!referenceCaptured)
        {
            cout
                << "Capture d'abord la reference "
                << "avec R."
                << endl;

            continue;
        }


        cout
            << "\nCapture : "
            << state
            << endl;


        Mat averagedDepth =
            captureAverageDepth(
                depthStream
            );


        // ----------------------------------------------------
        // Conversion RGB -> Depth pour la mesure
        // ----------------------------------------------------

        const double scaleX =
            static_cast<double>(
                averagedDepth.cols
            )
            /
            static_cast<double>(
                currentColor.cols
            );


        const double scaleY =
            static_cast<double>(
                averagedDepth.rows
            )
            /
            static_cast<double>(
                currentColor.rows
            );


        Point depthPoint(
            static_cast<int>(
                lround(
                    selectedColorPoint.x
                    *
                    scaleX
                )
            ),

            static_cast<int>(
                lround(
                    selectedColorPoint.y
                    *
                    scaleY
                )
            )
        );


        depthPoint.x =
            clamp(
                depthPoint.x,
                0,
                averagedDepth.cols - 1
            );

        depthPoint.y =
            clamp(
                depthPoint.y,
                0,
                averagedDepth.rows - 1
            );


        const Rect depthROI =
            createROI(
                depthPoint,
                averagedDepth.cols,
                averagedDepth.rows
            );


        const Measurement measurement =
            calculateMeasurement(
                state,
                referenceDepth,
                averagedDepth,
                depthROI
            );


        printMeasurement(
            measurement
        );


        saveMeasurement(
            csvPath,
            measurement
        );


        cout
            << "CSV : "
            << csvPath
            << endl;
    }


    // ========================================================
    // CLEANUP
    // ========================================================

    destroyAllWindows();


    colorStream.stop();
    colorStream.destroy();

    depthStream.stop();
    depthStream.destroy();

    device.close();

    OpenNI::shutdown();


    cout
        << "Experiment termine."
        << endl;


    return 0;
}