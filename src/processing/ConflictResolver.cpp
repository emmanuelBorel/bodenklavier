#include "ConflictResolver.h"

#include <algorithm>
#include <vector>

using namespace std;


vector<int> ConflictResolver::resolve(
    const vector<int>& detectedKeys,
    const vector<PianoKey>& keys
) const
{
    vector<int> result;


    // ========================================================
    // 1. Nettoyer les indices recus
    // ========================================================

    vector<int> validDetectedKeys;

    for (int index : detectedKeys)
    {
        if (
            index >= 0
            &&
            index < static_cast<int>(keys.size())
        )
        {
            // Eviter les doublons
            if (
                find(
                    validDetectedKeys.begin(),
                    validDetectedKeys.end(),
                    index
                )
                ==
                validDetectedKeys.end()
            )
            {
                validDetectedKeys.push_back(index);
            }
        }
    }


    // ========================================================
    // 2. Conserver toutes les touches noires detectees
    // ========================================================

    for (int index : validDetectedKeys)
    {
        if (keys[index].isBlack)
        {
            result.push_back(index);
        }
    }


    // ========================================================
    // 3. Examiner les touches blanches
    // ========================================================

    for (int whiteIndex : validDetectedKeys)
    {
        if (keys[whiteIndex].isBlack)
        {
            continue;
        }


        bool conflictWithBlackKey = false;


        for (int blackIndex : validDetectedKeys)
        {
            if (!keys[blackIndex].isBlack)
            {
                continue;
            }


            const cv::Rect& whiteZone =
                keys[whiteIndex].zone;

            const cv::Rect& blackZone =
                keys[blackIndex].zone;


            // Intersection geometrique entre
            // la touche blanche et la touche noire.
            cv::Rect intersection =
                whiteZone & blackZone;


            if (intersection.area() > 0)
            {
                conflictWithBlackKey = true;

                break;
            }
        }


        // Pas de conflit avec une noire detectee :
        // on garde la blanche.
        if (!conflictWithBlackKey)
        {
            result.push_back(whiteIndex);
        }
    }


    // ========================================================
    // 4. Garder un ordre stable
    // ========================================================

    sort(
        result.begin(),
        result.end()
    );


    return result;
}