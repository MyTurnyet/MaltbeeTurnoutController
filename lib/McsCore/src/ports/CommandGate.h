#pragma once

class CommandGate
{
public:
    virtual ~CommandGate() = default;
    virtual bool open() const = 0;
};
