#pragma once

class SetupModeTrigger
{
public:
    virtual ~SetupModeTrigger() = default;
    virtual bool requested() const = 0;
};
