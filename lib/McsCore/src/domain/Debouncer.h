#pragma once

#include "domain/Level.h"
#include "domain/Instant.h"
#include "domain/Duration.h"

class Debouncer
{
public:
    Debouncer(Level initialLevel, Duration stableDuration)
        : stableLevel_(initialLevel),
          candidateLevel_(initialLevel),
          candidateSince_(Instant(0)),
          stableDuration_(stableDuration)
    {
    }

    void sample(Level level, Instant now)
    {
        if (level != candidateLevel_)
        {
            candidateLevel_ = level;
            candidateSince_ = now;
        }

        if (candidateLevel_ != stableLevel_ && (now - candidateSince_) >= stableDuration_)
        {
            stableLevel_ = candidateLevel_;
        }
    }

    Level stable() const
    {
        return stableLevel_;
    }

private:
    Level stableLevel_;
    Level candidateLevel_;
    Instant candidateSince_;
    Duration stableDuration_;
};
