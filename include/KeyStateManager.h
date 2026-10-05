#ifndef KEY_STATE_MANAGER_H
#define KEY_STATE_MANAGER_H

#include <vector>
#include <string>

#include "PianoLayout.h"


enum class KeyEventType
{
    PRESS,
    HOLD,
    RELEASE
};


struct KeyEvent
{
    int keyIndex;
    std::string note;
    KeyEventType type;
};


class KeyStateManager
{
public:

    explicit KeyStateManager(
        const std::vector<PianoKey>& keys
    );

    std::vector<KeyEvent> update(
        const std::vector<int>& currentlyTouchedKeys
    );

    void reset();


private:

    std::vector<PianoKey> keys;

    // Etat stable confirme
    std::vector<bool> stableState;

    // Nombre de frames consecutives detectees
    std::vector<int> detectedFrames;

    // Nombre de frames consecutives non detectees
    std::vector<int> missingFrames;

    // Nombre de frames necessaires avant PRESS
    int pressConfirmationFrames;

    // Nombre de frames necessaires avant RELEASE
    int releaseConfirmationFrames;
};


#endif