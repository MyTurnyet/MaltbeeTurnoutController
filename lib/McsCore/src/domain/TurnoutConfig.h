#pragma once

#include "domain/TurnoutId.h"
#include "domain/TurnoutPosition.h"
#include "domain/Orientation.h"
#include "domain/Duration.h"

class TurnoutConfig
{
public:
    TurnoutConfig(TurnoutId id,
                  int outputPin,
                  int feedbackPin,
                  Orientation orientation,
                  Duration settleDuration,
                  Duration movementTimeout)
        : id_(id),
          outputPin_(outputPin),
          feedbackPin_(feedbackPin),
          orientation_(orientation),
          settleDuration_(settleDuration),
          movementTimeout_(movementTimeout)
    {
    }

    TurnoutId id() const
    {
        return id_;
    }

    int outputPin() const
    {
        return outputPin_;
    }

    int feedbackPin() const
    {
        return feedbackPin_;
    }

    Orientation orientation() const
    {
        return orientation_;
    }

    Duration settleDuration() const
    {
        return settleDuration_;
    }

    Duration movementTimeout() const
    {
        return movementTimeout_;
    }

    bool operator==(const TurnoutConfig& other) const
    {
        // Orientation has no operator== (out of this task's scope to add);
        // toLevel at a fixed input fully distinguishes its two possible states.
        return id_ == other.id_
            && outputPin_ == other.outputPin_
            && feedbackPin_ == other.feedbackPin_
            && orientation_.toLevel(TurnoutPosition::closed()) == other.orientation_.toLevel(TurnoutPosition::closed())
            && settleDuration_ == other.settleDuration_
            && movementTimeout_ == other.movementTimeout_;
    }

    bool operator!=(const TurnoutConfig& other) const
    {
        return !(*this == other);
    }

private:
    TurnoutId id_;
    int outputPin_;
    int feedbackPin_;
    Orientation orientation_;
    Duration settleDuration_;
    Duration movementTimeout_;
};
