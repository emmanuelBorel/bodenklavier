#include <OpenNI.h>
#include <opencv2/opencv.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

using namespace openni;
namespace fs = std::filesystem;

// ============================================================
// CONFIGURATION DE L'EXPERIENCE
// ============================================================

const int NUMBER_OF_FRAMES = 30;

// Seuil uniquement descriptif.
// Ce n'est PAS encore notre seuil de contact.
const int CHANGE_THRESHOLD_MM = 30;

const int MAX_TRIALS = 10;

const std::string OUTPUT_DIR =
    "evaluation/bodenkontakt/main";

const std::string ROI_OUTPUT_DIR =
    "evaluation/bodenkontakt/main/roi";

const std::string CSV_FILE =
    "evaluation/bodenkontakt/main/bodenkontakt_main.csv";

// ============================================================
// ETATS EXPERIMENTAUX
// ============================================================

const std::vector<std::string> STATES = {
    "EMPTY",
    "HOVER_200MM",
    "HOVER_100MM",
    "HOVER_50MM",
    "CONTACT"
};

// ============================================================
// VARIABLES ROI
// ============================================================

cv::Rect selectedColorROI;
bool roiSelected = false;

cv::Point dragStart;
bool dragging = false;

// ============================================================
// STRUCTURE MESURE
// ============================================================

struct Measurement {
    int validPixels = 0;
    int changedPixels = 0;

    double changedRatio = 0.0;

    double meanSigned = 0.0;
    double medianSigned = 0.0;

    double meanPositive = 0.0;
    double medianPositive = 0.0;

    double p10 = 0.0;
    double p25 = 0.0;
    double p50 = 0.0;
    double p75 = 0.0;
    double p90 = 0.0;

    double minDifference = 0.0;
    double maxDifference = 0.0;
};

// ============================================================
// OUTILS STATISTIQUES
// ============================================================

double percentile(std::vector<double> values, double p)
{
    if (values.empty())
        return 0.0;

    std::sort(values.begin(), values.end());

    double index = p * (values.size() - 1);

    size_t lower = static_cast<size_t>(std::floor(index));
    size_t upper = static_cast<size_t>(std::ceil(index));

    if (lower == upper)
        return values[lower];

    double fraction = index - lower;

    return values[lower] * (1.0 - fraction)
         + values[upper] * fraction;
}

double mean(const std::vector<double>& values)
{
    if (values.empty())
        return 0.0;

    return std::accumulate(
        values.begin(),
        values.end(),
        0.0
    ) / static_cast<double>(values.size());
}

// ============================================================
// SOURIS : SELECTION RECTANGULAIRE DE LA ROI
// ============================================================

void mouseCallback(int event, int x, int y, int, void*)
{
    if (event == cv::EVENT_LBUTTONDOWN)
    {
        dragging = true;
        dragStart = cv::Point(x, y);

        selectedColorROI =
            cv::Rect(x, y, 0, 0);
    }

    else if (event == cv::EVENT_MOUSEMOVE && dragging)
    {
        int x1 = std::min(dragStart.x, x);
        int y1 = std::min(dragStart.y, y);

        int x2 = std::max(dragStart.x, x);
        int y2 = std::max(dragStart.y, y);

        selectedColorROI =
            cv::Rect(
                x1,
                y1,
                x2 - x1,
                y2 - y1
            );
    }

    else if (event == cv::EVENT_LBUTTONUP)
    {
        dragging = false;

        int x1 = std::min(dragStart.x, x);
        int y1 = std::min(dragStart.y, y);

        int x2 = std::max(dragStart.x, x);
        int y2 = std::max(dragStart.y, y);

        selectedColorROI =
            cv::Rect(
                x1,
                y1,
                x2 - x1,
                y2 - y1
            );

        if (selectedColorROI.width >= 10 &&
            selectedColorROI.height >= 10)
        {
            roiSelected = true;

            std::cout
                << "\nROI RGB selectionnee : "
                << "x=" << selectedColorROI.x
                << ", y=" << selectedColorROI.y
                << ", w=" << selectedColorROI.width
                << ", h=" << selectedColorROI.height
                << "\n";
        }
        else
        {
            roiSelected = false;

            std::cout
                << "\nROI trop petite. "
                << "Selectionne une zone plus grande.\n";
        }
    }
}

// ============================================================
// LECTURE DEPTH
// ============================================================

bool readDepthFrame(VideoStream& depthStream, cv::Mat& depth)
{
    VideoFrameRef frame;

    if (depthStream.readFrame(&frame) != STATUS_OK)
        return false;

    if (!frame.isValid())
        return false;

    cv::Mat temp(
        frame.getHeight(),
        frame.getWidth(),
        CV_16U,
        const_cast<void*>(frame.getData())
    );

    depth = temp.clone();

    return true;
}

// ============================================================
// LECTURE RGB
// ============================================================

bool readColorFrame(VideoStream& colorStream, cv::Mat& bgr)
{
    VideoFrameRef frame;

    if (colorStream.readFrame(&frame) != STATUS_OK)
        return false;

    if (!frame.isValid())
        return false;

    cv::Mat rgb(
        frame.getHeight(),
        frame.getWidth(),
        CV_8UC3,
        const_cast<void*>(frame.getData())
    );

    cv::cvtColor(
        rgb,
        bgr,
        cv::COLOR_RGB2BGR
    );

    return true;
}

// ============================================================
// MOYENNE DE 30 FRAMES DEPTH
// ============================================================

cv::Mat captureAverageDepth(VideoStream& depthStream)
{
    cv::Mat sum;
    cv::Mat count;

    int captured = 0;

    while (captured < NUMBER_OF_FRAMES)
    {
        cv::Mat depth;

        if (!readDepthFrame(depthStream, depth))
            continue;

        if (sum.empty())
        {
            sum = cv::Mat::zeros(
                depth.size(),
                CV_64F
            );

            count = cv::Mat::zeros(
                depth.size(),
                CV_32S
            );
        }

        for (int y = 0; y < depth.rows; ++y)
        {
            for (int x = 0; x < depth.cols; ++x)
            {
                uint16_t value =
                    depth.at<uint16_t>(y, x);

                if (value == 0)
                    continue;

                sum.at<double>(y, x) += value;
                count.at<int>(y, x)++;
            }
        }

        captured++;
    }

    cv::Mat average(
        sum.size(),
        CV_16U,
        cv::Scalar(0)
    );

    for (int y = 0; y < average.rows; ++y)
    {
        for (int x = 0; x < average.cols; ++x)
        {
            int n = count.at<int>(y, x);

            if (n > 0)
            {
                average.at<uint16_t>(y, x) =
                    static_cast<uint16_t>(
                        std::round(
                            sum.at<double>(y, x) / n
                        )
                    );
            }
        }
    }

    return average;
}

// ============================================================
// CONVERSION ROI RGB -> DEPTH
// ============================================================

cv::Rect convertColorROIToDepth(
    const cv::Rect& colorROI,
    const cv::Size& colorSize,
    const cv::Size& depthSize)
{
    double scaleX =
        static_cast<double>(depthSize.width)
        / colorSize.width;

    double scaleY =
        static_cast<double>(depthSize.height)
        / colorSize.height;

    int x =
        static_cast<int>(
            std::round(colorROI.x * scaleX)
        );

    int y =
        static_cast<int>(
            std::round(colorROI.y * scaleY)
        );

    int width =
        static_cast<int>(
            std::round(colorROI.width * scaleX)
        );

    int height =
        static_cast<int>(
            std::round(colorROI.height * scaleY)
        );

    cv::Rect result(
        x,
        y,
        width,
        height
    );

    cv::Rect imageBounds(
        0,
        0,
        depthSize.width,
        depthSize.height
    );

    return result & imageBounds;
}

// ============================================================
// CALCUL MESURE
// ============================================================

Measurement calculateMeasurement(
    const cv::Mat& referenceDepth,
    const cv::Mat& currentDepth,
    const cv::Rect& roi,
    cv::Mat& differenceImage)
{
    Measurement result;

    std::vector<double> signedDifferences;
    std::vector<double> positiveDifferences;

    differenceImage =
        cv::Mat::zeros(
            currentDepth.size(),
            CV_8U
        );

    for (int y = roi.y;
         y < roi.y + roi.height;
         ++y)
    {
        for (int x = roi.x;
             x < roi.x + roi.width;
             ++x)
        {
            uint16_t referenceValue =
                referenceDepth.at<uint16_t>(y, x);

            uint16_t currentValue =
                currentDepth.at<uint16_t>(y, x);

            if (referenceValue == 0 ||
                currentValue == 0)
            {
                continue;
            }

            double difference =
                static_cast<double>(referenceValue)
                - static_cast<double>(currentValue);

            result.validPixels++;

            signedDifferences.push_back(
                difference
            );

            if (difference > 0.0)
            {
                positiveDifferences.push_back(
                    difference
                );
            }

            if (difference >
                CHANGE_THRESHOLD_MM)
            {
                result.changedPixels++;
            }

            double displayValue =
                std::clamp(
                    difference,
                    0.0,
                    500.0
                );

            differenceImage.at<uint8_t>(y, x) =
                static_cast<uint8_t>(
                    displayValue / 500.0 * 255.0
                );
        }
    }

    if (result.validPixels > 0)
    {
        result.changedRatio =
            static_cast<double>(
                result.changedPixels
            )
            / result.validPixels;
    }

    if (!signedDifferences.empty())
    {
        result.meanSigned =
            mean(signedDifferences);

        result.medianSigned =
            percentile(
                signedDifferences,
                0.50
            );
    }

    if (!positiveDifferences.empty())
    {
        result.meanPositive =
            mean(positiveDifferences);

        result.medianPositive =
            percentile(
                positiveDifferences,
                0.50
            );

        result.p10 =
            percentile(
                positiveDifferences,
                0.10
            );

        result.p25 =
            percentile(
                positiveDifferences,
                0.25
            );

        result.p50 =
            percentile(
                positiveDifferences,
                0.50
            );

        result.p75 =
            percentile(
                positiveDifferences,
                0.75
            );

        result.p90 =
            percentile(
                positiveDifferences,
                0.90
            );

        auto minmax =
            std::minmax_element(
                positiveDifferences.begin(),
                positiveDifferences.end()
            );

        result.minDifference =
            *minmax.first;

        result.maxDifference =
            *minmax.second;
    }

    return result;
}

// ============================================================
// CSV
// ============================================================

void initializeCSV()
{
    fs::create_directories(
        OUTPUT_DIR
    );

    fs::create_directories(
        ROI_OUTPUT_DIR
    );

    if (fs::exists(CSV_FILE))
        return;

    std::ofstream file(CSV_FILE);

    file
        << "trial,"
        << "state,"
        << "valid_pixels,"
        << "changed_pixels,"
        << "changed_ratio,"
        << "mean_signed_mm,"
        << "median_signed_mm,"
        << "mean_positive_mm,"
        << "median_positive_mm,"
        << "p10_positive_mm,"
        << "p25_positive_mm,"
        << "p50_positive_mm,"
        << "p75_positive_mm,"
        << "p90_positive_mm,"
        << "min_positive_mm,"
        << "max_positive_mm\n";
}

void appendCSV(
    int trial,
    const std::string& state,
    const Measurement& m)
{
    std::ofstream file(
        CSV_FILE,
        std::ios::app
    );

    file
        << trial << ","
        << state << ","
        << m.validPixels << ","
        << m.changedPixels << ","
        << m.changedRatio << ","
        << m.meanSigned << ","
        << m.medianSigned << ","
        << m.meanPositive << ","
        << m.medianPositive << ","
        << m.p10 << ","
        << m.p25 << ","
        << m.p50 << ","
        << m.p75 << ","
        << m.p90 << ","
        << m.minDifference << ","
        << m.maxDifference
        << "\n";
}

void loadTrialCounters(
    std::map<std::string, int>& trialCounters)
{
    // Initialisation à zéro
    for (const auto& state : STATES)
    {
        trialCounters[state] = 0;
    }

    if (!fs::exists(CSV_FILE))
    {
        std::cout
            << "\nAucun CSV existant : "
            << "nouvelle campagne.\n";

        return;
    }

    std::ifstream file(CSV_FILE);

    if (!file.is_open())
    {
        std::cerr
            << "\nImpossible de lire : "
            << CSV_FILE
            << "\n";

        return;
    }

    std::string line;

    // Ignorer l'en-tête
    std::getline(file, line);

    while (std::getline(file, line))
    {
        if (line.empty())
            continue;

        std::stringstream ss(line);

        std::string trialString;
        std::string state;

        // Colonne 1 = trial
        std::getline(ss, trialString, ',');

        // Colonne 2 = state
        std::getline(ss, state, ',');

        if (trialString.empty() || state.empty())
            continue;

        try
        {
            int trial = std::stoi(trialString);

            // On ne considère que les états connus
            if (trialCounters.find(state)
                != trialCounters.end())
            {
                trialCounters[state] =
                    std::max(
                        trialCounters[state],
                        trial
                    );
            }
        }
        catch (...)
        {
            std::cerr
                << "Ligne CSV ignoree : "
                << line
                << "\n";
        }
    }

    std::cout
        << "\n====================================\n"
        << "REPRISE DE LA CAMPAGNE\n"
        << "====================================\n";

    for (const auto& state : STATES)
    {
        std::cout
            << std::left
            << std::setw(15)
            << state
            << " : "
            << trialCounters[state]
            << "/"
            << MAX_TRIALS
            << "\n";
    }

    std::cout
        << "====================================\n";
}

// ============================================================
// NOM IMAGE
// ============================================================

std::string makeImageFilename(
    int trial,
    const std::string& state)
{
    std::ostringstream name;

    name
        << ROI_OUTPUT_DIR
        << "/trial"
        << std::setw(2)
        << std::setfill('0')
        << trial
        << "_"
        << state
        << ".png";

    return name.str();
}

// ============================================================
// AFFICHAGE RESULTAT
// ============================================================

void printMeasurement(
    int trial,
    const std::string& state,
    const Measurement& m)
{
    std::cout
        << "\n====================================\n";

    std::cout
        << "TRIAL : "
        << trial
        << "/"
        << MAX_TRIALS
        << "\n";

    std::cout
        << "ETAT  : "
        << state
        << "\n";

    std::cout
        << "====================================\n";

    std::cout
        << "Pixels valides      : "
        << m.validPixels
        << "\n";

    std::cout
        << "Pixels modifies     : "
        << m.changedPixels
        << "\n";

    std::cout
        << "Ratio modifie       : "
        << std::fixed
        << std::setprecision(3)
        << m.changedRatio * 100.0
        << " %\n";

    std::cout
        << "Mean signed         : "
        << m.meanSigned
        << " mm\n";

    std::cout
        << "Median signed       : "
        << m.medianSigned
        << " mm\n";

    std::cout
        << "Mean positive       : "
        << m.meanPositive
        << " mm\n";

    std::cout
        << "Median positive     : "
        << m.medianPositive
        << " mm\n";

    std::cout
        << "P10 / P25 / P50     : "
        << m.p10 << " / "
        << m.p25 << " / "
        << m.p50
        << " mm\n";

    std::cout
        << "P75 / P90           : "
        << m.p75 << " / "
        << m.p90
        << " mm\n";

    std::cout
        << "Min / Max positive  : "
        << m.minDifference << " / "
        << m.maxDifference
        << " mm\n";

    std::cout
        << "====================================\n";
}

// ============================================================
// MAIN
// ============================================================

int main()
{
    initializeCSV();

    Status rc = OpenNI::initialize();

    if (rc != STATUS_OK)
    {
        std::cerr
            << "Erreur OpenNI : "
            << OpenNI::getExtendedError()
            << "\n";

        return 1;
    }

    Device device;

    rc = device.open(ANY_DEVICE);

    if (rc != STATUS_OK)
    {
        std::cerr
            << "Impossible d'ouvrir Astra.\n"
            << OpenNI::getExtendedError()
            << "\n";

        OpenNI::shutdown();

        return 1;
    }

    VideoStream depthStream;
    VideoStream colorStream;

    // --------------------------------------------------------
    // DEPTH
    // --------------------------------------------------------

    rc = depthStream.create(
        device,
        SENSOR_DEPTH
    );

    if (rc != STATUS_OK)
    {
        std::cerr
            << "Impossible de creer DepthStream.\n";

        device.close();
        OpenNI::shutdown();

        return 1;
    }

    rc = depthStream.start();

    if (rc != STATUS_OK)
    {
        std::cerr
            << "Impossible de demarrer DepthStream.\n";

        depthStream.destroy();
        device.close();
        OpenNI::shutdown();

        return 1;
    }

    // --------------------------------------------------------
    // COLOR
    // --------------------------------------------------------

    rc = colorStream.create(
        device,
        SENSOR_COLOR
    );

    if (rc != STATUS_OK)
    {
        std::cerr
            << "Impossible de creer ColorStream.\n";

        depthStream.stop();
        depthStream.destroy();
        device.close();
        OpenNI::shutdown();

        return 1;
    }

    rc = colorStream.start();

    if (rc != STATUS_OK)
    {
        std::cerr
            << "Impossible de demarrer ColorStream.\n";

        colorStream.destroy();
        depthStream.stop();
        depthStream.destroy();
        device.close();
        OpenNI::shutdown();

        return 1;
    }

    // --------------------------------------------------------
    // REGISTRATION
    // --------------------------------------------------------

    if (device.isImageRegistrationModeSupported(
            IMAGE_REGISTRATION_DEPTH_TO_COLOR))
    {
        rc = device.setImageRegistrationMode(
            IMAGE_REGISTRATION_DEPTH_TO_COLOR
        );

        if (rc == STATUS_OK)
        {
            std::cout
                << "Registration Depth -> Color : ON\n";
        }
        else
        {
            std::cout
                << "Registration Depth -> Color : ECHEC\n";
        }
    }
    else
    {
        std::cout
            << "Registration non supportee.\n";
    }

    // --------------------------------------------------------
    // STABILISATION
    // --------------------------------------------------------

    std::cout
        << "\nStabilisation de la camera...\n";

    for (int i = 0; i < 30; ++i)
    {
        cv::Mat tempDepth;
        cv::Mat tempColor;

        readDepthFrame(
            depthStream,
            tempDepth
        );

        readColorFrame(
            colorStream,
            tempColor
        );
    }

    // --------------------------------------------------------
    // WINDOWS
    // --------------------------------------------------------

    cv::namedWindow(
        "Bodenkontakt - RGB",
        cv::WINDOW_NORMAL
    );

    cv::namedWindow(
        "Bodenkontakt - Depth",
        cv::WINDOW_NORMAL
    );

    cv::setMouseCallback(
        "Bodenkontakt - RGB",
        mouseCallback
    );

    cv::Mat referenceDepth;

    std::map<std::string, int> trialCounters;

    loadTrialCounters(trialCounters);

    std::cout
        << "\n====================================\n"
        << "BODENKONTAKT - CAMPAGNE PRINCIPALE\n"
        << "====================================\n"
        << "Souris : dessiner ROI sur RGB\n"
        << "R      : reference tapis vide\n"
        << "1      : EMPTY\n"
        << "2      : HOVER_200MM\n"
        << "3      : HOVER_100MM\n"
        << "4      : HOVER_50MM\n"
        << "5      : CONTACT\n"
        << "Q      : quitter\n"
        << "====================================\n";

    bool running = true;

    while (running)
    {
        cv::Mat depth;
        cv::Mat color;

        if (!readDepthFrame(
                depthStream,
                depth))
        {
            continue;
        }

        if (!readColorFrame(
                colorStream,
                color))
        {
            continue;
        }

        cv::Mat colorDisplay =
            color.clone();

        // ----------------------------------------------------
        // ROI RGB
        // ----------------------------------------------------

        if (selectedColorROI.width > 0 &&
            selectedColorROI.height > 0)
        {
            cv::rectangle(
                colorDisplay,
                selectedColorROI,
                cv::Scalar(0, 255, 0),
                2
            );
        }

        // ----------------------------------------------------
        // DEPTH VISUALISATION
        // ----------------------------------------------------

        cv::Mat depth8;
        cv::Mat depthColor;

        double minValue = 0.0;
        double maxValue = 0.0;

        cv::minMaxLoc(
            depth,
            &minValue,
            &maxValue
        );

        if (maxValue > 0.0)
        {
            depth.convertTo(
                depth8,
                CV_8U,
                255.0 / maxValue
            );
        }
        else
        {
            depth8 =
                cv::Mat::zeros(
                    depth.size(),
                    CV_8U
                );
        }

        cv::applyColorMap(
            depth8,
            depthColor,
            cv::COLORMAP_JET
        );

        // ----------------------------------------------------
        // CONVERSION ROI
        // ----------------------------------------------------

        cv::Rect depthROI;

        if (roiSelected)
        {
            depthROI =
                convertColorROIToDepth(
                    selectedColorROI,
                    color.size(),
                    depth.size()
                );

            if (depthROI.width > 0 &&
                depthROI.height > 0)
            {
                cv::rectangle(
                    depthColor,
                    depthROI,
                    cv::Scalar(255, 255, 255),
                    2
                );
            }
        }

        cv::imshow(
            "Bodenkontakt - RGB",
            colorDisplay
        );

        cv::imshow(
            "Bodenkontakt - Depth",
            depthColor
        );

        int key =
            cv::waitKey(1) & 0xFF;

        // ----------------------------------------------------
        // QUITTER
        // ----------------------------------------------------

        if (key == 'q' || key == 'Q')
        {
            running = false;
            continue;
        }

        // ----------------------------------------------------
        // REFERENCE
        // ----------------------------------------------------

        if (key == 'r' || key == 'R')
        {
            if (!roiSelected)
            {
                std::cout
                    << "\nSelectionne d'abord une ROI.\n";

                continue;
            }

            std::cout
                << "\nCapture reference...\n"
                << "Le tapis doit etre vide.\n";

            referenceDepth =
                captureAverageDepth(
                    depthStream
                );

            std::cout
                << "Reference enregistree sur "
                << NUMBER_OF_FRAMES
                << " frames.\n";

            continue;
        }

        // ----------------------------------------------------
        // MESURE 1..5
        // ----------------------------------------------------

        if (key >= '1' && key <= '5')
        {
            if (!roiSelected)
            {
                std::cout
                    << "\nErreur : ROI non selectionnee.\n";

                continue;
            }

            if (referenceDepth.empty())
            {
                std::cout
                    << "\nErreur : capture d'abord "
                    << "la reference avec R.\n";

                continue;
            }

            int stateIndex =
                key - '1';

            std::string state =
                STATES[stateIndex];

            if (trialCounters[state] >= MAX_TRIALS)
            {
                std::cout
                    << "\n"
                    << state
                    << " possede deja "
                    << MAX_TRIALS
                    << " mesures.\n";

                continue;
            }

            int trial =
                ++trialCounters[state];

            std::cout
                << "\nCapture : "
                << state
                << " | Trial "
                << trial
                << "/"
                << MAX_TRIALS
                << "\n";

            cv::Mat currentDepth =
                captureAverageDepth(
                    depthStream
                );

            cv::Rect currentDepthROI =
                convertColorROIToDepth(
                    selectedColorROI,
                    color.size(),
                    currentDepth.size()
                );

            cv::Mat differenceImage;

            Measurement measurement =
                calculateMeasurement(
                    referenceDepth,
                    currentDepth,
                    currentDepthROI,
                    differenceImage
                );

            printMeasurement(
                trial,
                state,
                measurement
            );

            appendCSV(
                trial,
                state,
                measurement
            );

            // ------------------------------------------------
            // SAUVEGARDE CARTE DE DIFFERENCE ROI
            // ------------------------------------------------

            cv::Mat croppedDifference =
                differenceImage(
                    currentDepthROI
                ).clone();

            cv::Mat enlarged;

            cv::resize(
                croppedDifference,
                enlarged,
                cv::Size(),
                4.0,
                4.0,
                cv::INTER_NEAREST
            );

            cv::Mat coloredDifference;

            cv::applyColorMap(
                enlarged,
                coloredDifference,
                cv::COLORMAP_JET
            );

            std::string imageFilename =
                makeImageFilename(
                    trial,
                    state
                );

            cv::imwrite(
                imageFilename,
                coloredDifference
            );

            std::cout
                << "CSV   : "
                << CSV_FILE
                << "\n";

            std::cout
                << "Image : "
                << imageFilename
                << "\n";
        }
    }

    // ========================================================
    // CLEANUP
    // ========================================================

    colorStream.stop();
    colorStream.destroy();

    depthStream.stop();
    depthStream.destroy();

    device.close();

    OpenNI::shutdown();

    cv::destroyAllWindows();

    std::cout
        << "\nExperiment termine.\n";

    return 0;
}