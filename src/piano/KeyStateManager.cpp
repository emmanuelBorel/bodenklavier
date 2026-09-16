#include "KeyStateManager.h"

#include <vector>
#include <algorithm>

using namespace std;


KeyStateManager::KeyStateManager(
    const vector<PianoKey>& keys
)
    : keys(keys),
      previousState(keys.size(), false)
{
}


vector<KeyEvent> KeyStateManager::update(
    const vector<int>& currentlyTouchedKeys
)
{
    vector<KeyEvent> events;

    vector<bool> currentState(
        keys.size(),
        false
    );


    // ========================================================
    // Construire l'etat actuel
    // ========================================================

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
            currentState[index] =
                true;
        }
    }


    // ========================================================
    // Comparaison avec l'etat precedent
    // ========================================================

    for (
        size_t i = 0;
        i < keys.size();
        ++i
    )
    {
        const bool wasPressed =
            previousState[i];

        const bool isPressed =
            currentState[i];


        // ----------------------------------------------------
        // Nouvelle pression
        // ----------------------------------------------------

        if (
            !wasPressed
            &&
            isPressed
        )
        {
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
        }


        // ----------------------------------------------------
        // Touche maintenue
        // ----------------------------------------------------

        else if (
            wasPressed
            &&
            isPressed
        )
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
        }


        // ----------------------------------------------------
        // Touche relachee
        // ----------------------------------------------------

        else if (
            wasPressed
            &&
            !isPressed
        )
        {
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
        }
    }


    // ========================================================
    // Sauvegarder l'etat actuel
    // ========================================================

    previousState =
        currentState;


    return events;
}


void KeyStateManager::reset()
{
    previousState.assign(
        keys.size(),
        false
    );
}