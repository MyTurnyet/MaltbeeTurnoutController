#pragma once

class TurnoutPosition
{
public:
    static TurnoutPosition closed()
    {
        return TurnoutPosition(Value::ClosedValue);
    }

    static TurnoutPosition thrown()
    {
        return TurnoutPosition(Value::ThrownValue);
    }

    TurnoutPosition opposite() const
    {
        return TurnoutPosition(value_ == Value::ClosedValue ? Value::ThrownValue : Value::ClosedValue);
    }

    bool operator==(const TurnoutPosition& other) const
    {
        return value_ == other.value_;
    }

    bool operator!=(const TurnoutPosition& other) const
    {
        return !(*this == other);
    }

private:
    enum class Value
    {
        ClosedValue,
        ThrownValue
    };

    explicit TurnoutPosition(Value value) : value_(value)
    {
    }

    Value value_;
};
