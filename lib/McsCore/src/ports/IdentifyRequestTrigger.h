#pragma once

class IdentifyRequestTrigger
{
public:
    virtual ~IdentifyRequestTrigger() = default;
    virtual bool requested() const = 0;
};
