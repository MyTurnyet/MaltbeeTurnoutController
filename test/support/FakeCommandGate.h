#pragma once

#include "ports/CommandGate.h"

class FakeCommandGate : public CommandGate
{
public:
    explicit FakeCommandGate(bool isOpen) : isOpen_(isOpen)
    {
    }

    void setOpen(bool isOpen)
    {
        isOpen_ = isOpen;
    }

    bool open() const override
    {
        return isOpen_;
    }

private:
    bool isOpen_;
};
