#pragma once

#include <cstdint>

class Duration
{
public:
    explicit Duration(uint32_t milliseconds) : milliseconds_(milliseconds)
    {
    }

    uint32_t milliseconds() const
    {
        return milliseconds_;
    }

    bool operator==(const Duration& other) const
    {
        return milliseconds_ == other.milliseconds_;
    }

    bool operator!=(const Duration& other) const
    {
        return !(*this == other);
    }

    bool operator<(const Duration& other) const
    {
        return milliseconds_ < other.milliseconds_;
    }

    bool operator<=(const Duration& other) const
    {
        return milliseconds_ <= other.milliseconds_;
    }

    bool operator>(const Duration& other) const
    {
        return milliseconds_ > other.milliseconds_;
    }

    bool operator>=(const Duration& other) const
    {
        return milliseconds_ >= other.milliseconds_;
    }

private:
    uint32_t milliseconds_;
};
