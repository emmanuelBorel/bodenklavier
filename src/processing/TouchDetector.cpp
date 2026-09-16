#include "TouchDetector.h"

#include <opencv2/opencv.hpp>
#include <vector>
#include <algorithm>

using namespace cv;
using namespace std;


TouchDetector::TouchDetector()
    : depthDifferenceThreshold(80),
      minimumChangedPixels(100)
{
}


void TouchDetector::setReferenceDepth(
    const Mat& depth
)
{
    if (depth.empty())
    {
        return;
    }

    depth.copyTo(referenceDepth);
}


bool TouchDetector::hasReference() const
{
    return !referenceDepth.empty();
}


vector<int> TouchDetector::detectTouchedKeys(
    const Mat& currentDepth,
    const vector<PianoKey>& keys
) const
{
    vector<int> rawTouchedKeys;
    vector<int> filteredTouchedKeys;


    // ========================================================
    // SECURITE
    // ========================================================

    if (
        !hasReference()
        ||
        currentDepth.empty()
    )
    {
        return filteredTouchedKeys;
    }


    if (
        currentDepth.size() != referenceDepth.size()
        ||
        currentDepth.type() != referenceDepth.type()
    )
    {
        return filteredTouchedKeys;
    }


    // ========================================================
    // PARAMETRES DE DETECTION
    // ========================================================

    // Difference minimale entre tapis vide et image actuelle
    // Valeurs en millimetres environ.
    const int whiteDepthThreshold = 80;
    const int blackDepthThreshold = 55;

    // Pourcentage minimal de pixels modifies.
    const float whiteChangedRatio = 0.10f;
    const float blackChangedRatio = 0.055f;

    // On agrandit legerement la zone de detection des
    // touches noires pour tolerer les petites erreurs
    // de calibration.
    const int blackPaddingX = 8;
    const int blackPaddingY = 5;

    // Echantillonnage
    const int samplingStep = 3;


    // ========================================================
    // 1. DETECTION BRUTE
    // ========================================================

    for (
        size_t i = 0;
        i < keys.size();
        ++i
    )
    {
        const PianoKey& key =
            keys[i];


        Rect zone =
            key.zone;


        // ----------------------------------------------------
        // Pour les touches noires :
        // petite marge supplementaire pour la detection
        // ----------------------------------------------------

        if (key.isBlack)
        {
            zone.x -= blackPaddingX;

            zone.y -= blackPaddingY;

            zone.width +=
                2 * blackPaddingX;

            zone.height +=
                2 * blackPaddingY;
        }


        // Limiter la zone aux dimensions de l'image
        zone &=
            Rect(
                0,
                0,
                currentDepth.cols,
                currentDepth.rows
            );


        if (
            zone.width <= 0
            ||
            zone.height <= 0
        )
        {
            continue;
        }


        int changedPixels = 0;
        int validPixels = 0;


        // ====================================================
        // ANALYSE DES PIXELS
        // ====================================================

        for (
            int y = zone.y;
            y < zone.y + zone.height;
            y += samplingStep
        )
        {
            const uint16_t* currentRow =
                currentDepth.ptr<uint16_t>(y);

            const uint16_t* referenceRow =
                referenceDepth.ptr<uint16_t>(y);


            for (
                int x = zone.x;
                x < zone.x + zone.width;
                x += samplingStep
            )
            {
                // ============================================
                // Touche blanche :
                //
                // ignorer les pixels appartenant
                // aux touches noires
                // ============================================

                if (!key.isBlack)
                {
                    bool insideBlackKey =
                        false;


                    for (
                        const PianoKey& otherKey :
                        keys
                    )
                    {
                        if (!otherKey.isBlack)
                        {
                            continue;
                        }


                        if (
                            otherKey.zone.contains(
                                Point(x, y)
                            )
                        )
                        {
                            insideBlackKey =
                                true;

                            break;
                        }
                    }


                    if (insideBlackKey)
                    {
                        continue;
                    }
                }


                const int currentValue =
                    static_cast<int>(
                        currentRow[x]
                    );


                const int referenceValue =
                    static_cast<int>(
                        referenceRow[x]
                    );


                // Depth invalide
                if (
                    currentValue == 0
                    ||
                    referenceValue == 0
                )
                {
                    continue;
                }


                validPixels++;


                const int difference =
                    referenceValue
                    -
                    currentValue;


                const int threshold =
                    key.isBlack
                        ? blackDepthThreshold
                        : whiteDepthThreshold;


                if (
                    difference >
                    threshold
                )
                {
                    changedPixels++;
                }
            }
        }


        // ====================================================
        // CALCUL DU POURCENTAGE
        // ========================================================

        if (validPixels == 0)
        {
            continue;
        }


        const float changedRatio =
            static_cast<float>(
                changedPixels
            )
            /
            static_cast<float>(
                validPixels
            );


        const float requiredRatio =
            key.isBlack
                ? blackChangedRatio
                : whiteChangedRatio;


        if (
            changedRatio >= requiredRatio
        )
        {
            rawTouchedKeys.push_back(
                static_cast<int>(i)
            );
        }
    }


    // ========================================================
    // 2. PRIORITE AUX TOUCHES NOIRES
    // ========================================================

    for (
        int index :
        rawTouchedKeys
    )
    {
        const PianoKey& currentKey =
            keys[index];


        // ----------------------------------------------------
        // Toute touche noire detectee est conservee
        // ----------------------------------------------------

        if (currentKey.isBlack)
        {
            filteredTouchedKeys.push_back(
                index
            );

            continue;
        }


        bool suppressWhite =
            false;


        // ----------------------------------------------------
        // Chercher une touche noire detectee proche
        // ----------------------------------------------------

        for (
            int otherIndex :
            rawTouchedKeys
        )
        {
            const PianoKey& blackKey =
                keys[otherIndex];


            if (!blackKey.isBlack)
            {
                continue;
            }


            // Centre horizontal de la touche noire
            const int blackCenterX =
                blackKey.zone.x
                +
                blackKey.zone.width / 2;


            // Si le centre de la touche noire se trouve
            // horizontalement dans la touche blanche,
            // on donne la priorite a la touche noire.
            if (
                blackCenterX
                    >=
                currentKey.zone.x
                &&
                blackCenterX
                    <
                currentKey.zone.x
                +
                currentKey.zone.width
            )
            {
                suppressWhite =
                    true;

                break;
            }


            // ------------------------------------------------
            // Comme une touche noire se trouve souvent entre
            // deux touches blanches, on teste aussi
            // l'intersection geometrie.
            // ------------------------------------------------

            Rect overlap =
                currentKey.zone
                &
                blackKey.zone;


            if (
                overlap.area() > 0
            )
            {
                suppressWhite =
                    true;

                break;
            }
        }


        if (!suppressWhite)
        {
            filteredTouchedKeys.push_back(
                index
            );
        }
    }


    return filteredTouchedKeys;
}