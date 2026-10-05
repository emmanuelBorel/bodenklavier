#ifndef NOTE_MAPPER_H
#define NOTE_MAPPER_H

#include <string>

class NoteMapper
{
public:
    std::string toMusicalNote(
        const std::string& physicalNote
    ) const;
};

#endif