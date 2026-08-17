#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/WebFormCommissioningAdapter.h"
#include "domain/CommissioningSession.h"
#include "domain/NodeConfig.h"
#include "domain/NodeId.h"
#include "domain/WifiCredentials.h"
#include "domain/BrokerAddress.h"
#include "domain/TurnoutConfig.h"
#include "domain/TurnoutId.h"
#include "domain/Orientation.h"
#include "domain/Duration.h"
#include "support/FakeConfigStore.h"

// NOTE on turnout pin numbers below: NodeConfig::factoryDefault() gives every
// turnout the same sentinel pins (-1 output, -1 feedback), so a submission
// that only fills SOME turnouts while the rest sit at that sentinel would
// trip NodeConfig::validate()'s pin-conflict check (multiple turnouts
// "sharing" pin -1) and fail to save. Test submissions that expect a
// successful save therefore give every one of the 8 turnouts a distinct,
// non-conflicting pin pair.

namespace
{
WebFormSubmission fullyValidSubmission()
{
    WebFormSubmission form;
    form.nodeId = "3";
    form.wifiSsid = "Layout Room";
    form.wifiPassword = "blue caboose 42";
    form.brokerHost = "192.168.1.50";
    form.brokerPort = "1883";

    for (size_t i = 0; i < form.turnouts.size(); ++i)
    {
        int outputPin = 10 + static_cast<int>(i) * 2;
        int feedbackPin = 11 + static_cast<int>(i) * 2;
        form.turnouts[i].pin = std::to_string(outputPin);
        form.turnouts[i].feedbackPin = std::to_string(feedbackPin);
        form.turnouts[i].orientation = "normal";
        form.turnouts[i].settleMs = "300";
        form.turnouts[i].timeoutMs = "4000";
    }

    return form;
}
}

TEST_CASE("WebFormCommissioningAdapter applies a fully valid submission and reboots")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    std::string response = adapter.submit(fullyValidSubmission());

    REQUIRE(response == "REBOOTING");
    REQUIRE(store.saved().has_value());
    REQUIRE(store.saved()->id().value() == 3);
    REQUIRE(store.saved()->wifi().ssid() == "Layout Room");
    REQUIRE(store.saved()->wifi().password() == "blue caboose 42");
    REQUIRE(store.saved()->broker().host() == "192.168.1.50");
    REQUIRE(store.saved()->broker().port() == 1883);
    REQUIRE(store.saved()->turnouts()[0].outputPin() == 10);
    REQUIRE(store.saved()->turnouts()[7].outputPin() == 24);
}

TEST_CASE("WebFormCommissioningAdapter leaves turnout slots with an empty pin unchanged")
{
    // Seed the store with an already-valid config (distinct pins across all
    // 8 turnouts) so leaving some turnout fields blank doesn't collide with
    // the factory-default -1/-1 sentinel - see the file-level NOTE above.
    FakeConfigStore store;
    Orientation orientation = Orientation::normal();
    Duration settle(50);
    Duration timeout(200);
    std::array<TurnoutConfig, 8> seedTurnouts{
        TurnoutConfig(TurnoutId(1), 21, 31, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(2), 22, 32, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(3), 23, 33, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(4), 24, 34, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(5), 25, 35, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(6), 26, 36, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(7), 27, 37, orientation, settle, timeout),
        TurnoutConfig(TurnoutId(8), 28, 38, orientation, settle, timeout)};
    NodeConfig seed(NodeId(5), WifiCredentials("existing", "pw"), BrokerAddress("10.0.0.1", 1883), seedTurnouts);
    store.save(seed);

    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    WebFormSubmission form;
    form.nodeId = "5";
    form.wifiSsid = "existing";
    form.wifiPassword = "pw";
    form.brokerHost = "10.0.0.1";
    form.brokerPort = "1883";
    // Only fill in turnout 1, with a pin distinct from every seeded turnout;
    // turnouts 2-8 stay unset (optional) and should keep their seeded pins.
    form.turnouts[0].pin = "12";
    form.turnouts[0].feedbackPin = "13";
    form.turnouts[0].orientation = "normal";
    form.turnouts[0].settleMs = "300";
    form.turnouts[0].timeoutMs = "4000";

    std::string response = adapter.submit(form);

    REQUIRE(response == "REBOOTING");
    REQUIRE(store.saved().has_value());
    REQUIRE(store.saved()->turnouts()[0].outputPin() == 12);
    REQUIRE(store.saved()->turnouts()[1].outputPin() == 22);
    REQUIRE(store.saved()->turnouts()[7].outputPin() == 28);
}

TEST_CASE("WebFormCommissioningAdapter stops at the first error and does not save")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    WebFormSubmission form = fullyValidSubmission();
    form.brokerPort = "not-a-number";

    std::string response = adapter.submit(form);

    REQUIRE(response.rfind("ERROR", 0) == 0);
    REQUIRE_FALSE(store.saved().has_value());
}

TEST_CASE("WebFormCommissioningAdapter reports a validation error from save and does not reboot")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    WebFormSubmission form = fullyValidSubmission();
    form.nodeId = "0"; // invalid: fails NodeConfig::validate()

    std::string response = adapter.submit(form);

    REQUIRE(response.rfind("ERROR", 0) == 0);
    REQUIRE_FALSE(store.saved().has_value());
}
