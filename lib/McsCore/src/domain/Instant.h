#pragma once

#include "domain/Duration.h"

class Instant
{
public:
    explicit Instant(unsigned long milliseconds) : milliseconds_(milliseconds)
    {
    }

    Duration operator-(const Instant& earlier) const
    {
        return Duration(milliseconds_ - earlier.milliseconds_);
    }

    Instant operator+(const Duration& duration) const
    {
        return Instant(milliseconds_ + duration.milliseconds());
    }

    bool operator==(const Instant& other) const
    {
        return milliseconds_ == other.milliseconds_;
    }

    bool operator!=(const Instant& other) const
    {
        return !(*this == other);
    }

    bool operator<(const Instant& other) const
    {
        return milliseconds_ < other.milliseconds_;
    }

    bool operator<=(const Instant& other) const
    {
        return milliseconds_ <= other.milliseconds_;
    }

    bool operator>(const Instant& other) const
    {
        return milliseconds_ > other.milliseconds_;
    }

    bool operator>=(const Instant& other) const
    {
        return milliseconds_ >= other.milliseconds_;
    }

private:
    unsigned long milliseconds_;
};
