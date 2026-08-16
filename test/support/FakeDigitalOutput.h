#pragma once

#include "ports/DigitalOutput.h"

class FakeDigitalOutput : public DigitalOutput
{
public:
    void write(Level level) override
    {
        level_ = level;
        writeCallCount_++;
    }

    Level level() const
    {
        return level_;
    }

    int writeCallCount() const
    {
        return writeCallCount_;
    }

private:
    Level level_ = Level::Low;
    int writeCallCount_ = 0;
};
