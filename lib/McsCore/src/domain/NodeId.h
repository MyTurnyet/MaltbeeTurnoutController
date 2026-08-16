#pragma once

class NodeId
{
public:
    explicit NodeId(int value) : value_(value)
    {
    }

    int value() const
    {
        return value_;
    }

    bool operator==(const NodeId& other) const
    {
        return value_ == other.value_;
    }

    bool operator!=(const NodeId& other) const
    {
        return !(*this == other);
    }

private:
    int value_;
};
