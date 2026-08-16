#pragma once

#include <vector>

#include "ports/PositionReporter.h"

struct ReportedPosition
{
    TurnoutId id;
    TurnoutState state;
};

class FakePositionReporter : public PositionReporter
{
public:
    void report(TurnoutId id, TurnoutState state) override
    {
        reports_.push_back(ReportedPosition{id, state});
    }

    const std::vector<ReportedPosition>& reports() const
    {
        return reports_;
    }

private:
    std::vector<ReportedPosition> reports_;
};
