#pragma once

#ifdef ARDUINO

#include <Preferences.h>

#include "ports/SetupModeRequestStore.h"

// Separate NVS namespace from NvsConfigStore's "mcs-cfg" - this is a
// transient boot-intent flag, not part of the node's actual configuration.
class NvsSetupModeRequestStore : public SetupModeRequestStore
{
public:
    void requestOnNextBoot() override
    {
        Preferences prefs;
        prefs.begin(kNamespace, false);
        prefs.putBool(kKey, true);
        prefs.end();
    }

    bool consumeRequest() override
    {
        Preferences prefs;
        prefs.begin(kNamespace, false);
        bool pending = prefs.getBool(kKey, false);
        if (pending)
        {
            prefs.putBool(kKey, false);
        }
        prefs.end();
        return pending;
    }

private:
    static constexpr const char* kNamespace = "mcs-boot";
    static constexpr const char* kKey = "wsetup";
};

#endif
