#pragma once

#include <optional>

#include "ports/DigitalInput.h"
#include "domain/Debouncer.h"
#include "domain/Orientation.h"
#include "domain/TurnoutPosition.h"
#include "domain/Level.h"
#include "domain/Instant.h"
#include "domain/Duration.h"

class FeedbackSensor
{
public:
    FeedbackSensor(DigitalInput& input, Orientation orientation, Duration stableDuration)
        : input_(input), orientation_(orientation), stableDuration_(stableDuration)
    {
    }

    void sample(Instant now)
    {
        Level level = input_.read();

        if (!debouncer_.has_value())
        {
            debouncer_.emplace(level, stableDuration_);
            firstSampleInstant_ = now;
        }
        else
        {
            debouncer_->sample(level, now);
        }

        lastSampleInstant_ = now;
    }

    std::optional<TurnoutPosition> observed() const
    {
        if (!debouncer_.has_value())
        {
            return std::nullopt;
        }

        if ((lastSampleInstant_ - firstSampleInstant_) < stableDuration_)
        {
            return std::nullopt;
        }

        return orientation_.toPosition(debouncer_->stable());
    }

private:
    DigitalInput& input_;
    Orientation orientation_;
    Duration stableDuration_;
    std::optional<Debouncer> debouncer_;
    Instant firstSampleInstant_ = Instant(0);
    Instant lastSampleInstant_ = Instant(0);
};
