#pragma once

#include <optional>

#include "domain/TurnoutPosition.h"
#include "domain/TurnoutState.h"
#include "domain/Deadline.h"
#include "domain/Instant.h"
#include "domain/Duration.h"

class TurnoutMotion
{
public:
    TurnoutMotion(TurnoutPosition initialPosition, Duration movementTimeout, Duration settleDuration)
        : target_(initialPosition),
          movementTimeout_(movementTimeout),
          settleDuration_(settleDuration),
          motionState_(MotionState::AtRest)
    {
    }

    void commandTo(TurnoutPosition position, Instant now)
    {
        target_ = position;
        motionState_ = MotionState::Moving;
        movementDeadline_.arm(now, movementTimeout_);
        settleDeadline_.disarm();
    }

    void update(std::optional<TurnoutPosition> observed, Instant now)
    {
        switch (motionState_)
        {
        case MotionState::Moving:
            if (observed.has_value() && *observed == target_)
            {
                motionState_ = MotionState::Settling;
                movementDeadline_.disarm();
                settleDeadline_.arm(now, settleDuration_);
            }
            else if (movementDeadline_.expired(now))
            {
                motionState_ = MotionState::Faulted;
            }
            break;

        case MotionState::Settling:
            if (settleDeadline_.expired(now))
            {
                motionState_ = MotionState::AtRest;
                settleDeadline_.disarm();
            }
            break;

        case MotionState::AtRest:
            if (observed.has_value() && *observed != target_)
            {
                motionState_ = MotionState::Faulted;
            }
            break;

        case MotionState::Faulted:
            if (observed.has_value() && *observed == target_)
            {
                motionState_ = MotionState::Settling;
                settleDeadline_.arm(now, settleDuration_);
            }
            break;
        }
    }

    TurnoutState state() const
    {
        switch (motionState_)
        {
        case MotionState::AtRest:
            return (target_ == TurnoutPosition::closed()) ? TurnoutState::Closed : TurnoutState::Thrown;
        case MotionState::Moving:
        case MotionState::Settling:
            return TurnoutState::Moving;
        case MotionState::Faulted:
            return TurnoutState::Unknown;
        }

        return TurnoutState::Unknown;
    }

private:
    enum class MotionState
    {
        AtRest,
        Moving,
        Settling,
        Faulted
    };

    TurnoutPosition target_;
    Duration movementTimeout_;
    Duration settleDuration_;
    MotionState motionState_;
    Deadline movementDeadline_;
    Deadline settleDeadline_;
};
