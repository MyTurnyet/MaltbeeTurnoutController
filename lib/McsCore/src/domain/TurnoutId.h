#pragma once

class TurnoutId
{
public:
    explicit TurnoutId(int value) : value_(value)
    {
    }

    int value() const
    {
        return value_;
    }

    bool operator==(const TurnoutId& other) const
    {
        return value_ == other.value_;
    }

    bool operator!=(const TurnoutId& other) const
    {
        return !(*this == other);
    }

private:
    int value_;
};
