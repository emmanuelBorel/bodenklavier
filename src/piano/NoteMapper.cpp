#include "NoteMapper.h"

#include <string>
#include <unordered_map>

using namespace std;


string NoteMapper::toMusicalNote(
    const string& physicalNote
) const
{
    // Les touches blanches portent deja
    // directement leur nom musical.
    if (
        physicalNote.rfind(
            "BLACK_",
            0
        ) != 0
    )
    {
        return physicalNote;
    }


    // Correspondance configurable entre
    // les touches noires physiques et
    // les notes musicales.
    static const unordered_map<string, string>
        blackKeyMapping = {

        {"BLACK_01", "Cs4"},
        {"BLACK_02", "Ds4"},
        {"BLACK_03", "Fs4"},
        {"BLACK_04", "Gs4"},
        {"BLACK_05", "As4"},

        {"BLACK_06", "Cs5"},
        {"BLACK_07", "Ds5"},
        {"BLACK_08", "Fs5"},
        {"BLACK_09", "Gs5"},
        {"BLACK_10", "As5"}
    };


    auto it =
        blackKeyMapping.find(
            physicalNote
        );


    if (
        it !=
        blackKeyMapping.end()
    )
    {
        return it->second;
    }


    // Securite :
    // identifiant inconnu.
    return physicalNote;
}