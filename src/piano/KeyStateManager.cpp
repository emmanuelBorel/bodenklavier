#include "KeyStateManager.h"

#include <vector>

using namespace std;


KeyStateManager::KeyStateManager(
    const vector<PianoKey>& keys
)
    : keys(keys),

      stableState(
          keys.size(),
          false
      ),

      detectedFrames(
          keys.size(),
          0
      ),

      missingFrames(
          keys.size(),
          0
      ),

      pressConfirmationFrames(3),

      releaseConfirmationFrames(4)
{
}


vector<KeyEvent> KeyStateManager::update(
    const vector<int>& currentlyTouchedKeys
)
{
    vector<KeyEvent> events;


    // ========================================================
    // 1. Construire l'etat brut de cette frame
    // ========================================================

    vector<bool> currentDetection(
        keys.size(),
        false
    );


    for (
        int index :
        currentlyTouchedKeys
    )
    {
        if (
            index >= 0
            &&
            index <
            static_cast<int>(
                keys.size()
            )
        )
        {
            currentDetection[index] =
                true;
        }
    }


    // ========================================================
    // 2. Analyser chaque touche
    // ========================================================

    for (
        size_t i = 0;
        i < keys.size();
        ++i
    )
    {
        // ====================================================
        // TOUCHE DETECTEE DANS LA FRAME ACTUELLE
        // ====================================================

        if (currentDetection[i])
        {
            detectedFrames[i]++;

            missingFrames[i] = 0;


            // ------------------------------------------------
            // La touche n'etait pas encore consideree
            // comme pressee.
            // ------------------------------------------------

            if (!stableState[i])
            {
                // Il faut plusieurs frames consecutives
                // avant de confirmer PRESS.

                if (
                    detectedFrames[i]
                    >=
                    pressConfirmationFrames
                )
                {
                    stableState[i] =
                        true;


                    KeyEvent event;

                    event.keyIndex =
                        static_cast<int>(i);

                    event.note =
                        keys[i].note;

                    event.type =
                        KeyEventType::PRESS;


                    events.push_back(
                        event
                    );


                    // Eviter que le compteur continue
                    // inutilement a augmenter.
                    detectedFrames[i] =
                        pressConfirmationFrames;
                }
            }

            // ------------------------------------------------
            // La touche est deja officiellement pressee.
            // ------------------------------------------------

            else
            {
                KeyEvent event;

                event.keyIndex =
                    static_cast<int>(i);

                event.note =
                    keys[i].note;

                event.type =
                    KeyEventType::HOLD;


                events.push_back(
                    event
                );


                detectedFrames[i] =
                    pressConfirmationFrames;
            }
        }


        // ====================================================
        // TOUCHE ABSENTE DE LA FRAME ACTUELLE
        // ====================================================

        else
        {
            missingFrames[i]++;

            detectedFrames[i] = 0;


            // ------------------------------------------------
            // Si elle etait officiellement pressee,
            // ne pas faire RELEASE immediatement.
            // ------------------------------------------------

            if (stableState[i])
            {
                if (
                    missingFrames[i]
                    >=
                    releaseConfirmationFrames
                )
                {
                    stableState[i] =
                        false;


                    KeyEvent event;

                    event.keyIndex =
                        static_cast<int>(i);

                    event.note =
                        keys[i].note;

                    event.type =
                        KeyEventType::RELEASE;


                    events.push_back(
                        event
                    );


                    missingFrames[i] = 0;
                }
            }

            // ------------------------------------------------
            // Elle etait deja relachee.
            // ------------------------------------------------

            else
            {
                // Eviter une croissance infinie du compteur.
                if (
                    missingFrames[i]
                    >
                    releaseConfirmationFrames
                )
                {
                    missingFrames[i] =
                        releaseConfirmationFrames;
                }
            }
        }
    }


    return events;
}


void KeyStateManager::reset()
{
    stableState.assign(
        keys.size(),
        false
    );


    detectedFrames.assign(
        keys.size(),
        0
    );


    missingFrames.assign(
        keys.size(),
        0
    );
}