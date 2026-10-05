#ifndef CONFLICT_RESOLVER_H
#define CONFLICT_RESOLVER_H

#include <vector>

#include "PianoLayout.h"


class ConflictResolver
{
public:

    ConflictResolver() = default;

    std::vector<int> resolve(
        const std::vector<int>& detectedKeys,
        const std::vector<PianoKey>& keys
    ) const;
};


#endif