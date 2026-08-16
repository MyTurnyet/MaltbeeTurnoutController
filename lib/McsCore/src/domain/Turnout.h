#pragma once

#include <optional>
#include <utility>

#include "domain/TurnoutId.h"
#include "domain/TurnoutPosition.h"
#include "domain/TurnoutState.h"
#include "domain/TurnoutMotion.h"
#include "domain/FeedbackSensor.h"
#include "domain/Orientation.h"
#include "domain/Instant.h"
#include "ports/DigitalOutput.h"
#include "ports/PositionReporter.h"

class Turnout
{
public:
    Turnout(TurnoutId id,
            DigitalOutput& output,
            Orientation orientation,
            FeedbackSensor sensor,
            TurnoutMotion motion,
            PositionReporter& reporter)
        : id_(id),
          output_(output),
          orientation_(orientation),
          sensor_(std::move(sensor)),
          motion_(std::move(motion)),
          reporter_(reporter)
    {
    }

    void moveTo(TurnoutPosition position, Instant now)
    {
        output_.write(orientation_.toLevel(position));
        motion_.commandTo(position, now);
    }

    void tick(Instant now)
    {
        sensor_.sample(now);
        motion_.update(sensor_.observed(), now);

        TurnoutState current = motion_.state();
        if (!lastReportedState_.has_value() || *lastReportedState_ != current)
        {
            reporter_.report(id_, current);
            lastReportedState_ = current;
        }
    }

    TurnoutId id() const
    {
        return id_;
    }

private:
    TurnoutId id_;
    DigitalOutput& output_;
    Orientation orientation_;
    FeedbackSensor sensor_;
    TurnoutMotion motion_;
    PositionReporter& reporter_;
    std::optional<TurnoutState> lastReportedState_;
};
