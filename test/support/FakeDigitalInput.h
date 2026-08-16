#pragma once

#include <queue>

#include "ports/DigitalInput.h"

class FakeDigitalInput : public DigitalInput
{
public:
    void enqueue(Level level)
    {
        levels_.push(level);
    }

    Level read() override
    {
        if (levels_.empty())
        {
            return lastLevel_;
        }

        lastLevel_ = levels_.front();
        levels_.pop();
        return lastLevel_;
    }

private:
    std::queue<Level> levels_;
    Level lastLevel_ = Level::Low;
};
