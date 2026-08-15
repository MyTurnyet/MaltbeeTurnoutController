#pragma once

class Duration
{
public:
    explicit Duration(unsigned long milliseconds) : milliseconds_(milliseconds)
    {
    }

    unsigned long milliseconds() const
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
    unsigned long milliseconds_;
};
