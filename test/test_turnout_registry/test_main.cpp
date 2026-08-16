#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <utility>

#include "domain/TurnoutRegistry.h"
#include "domain/Turnout.h"
#include "domain/TurnoutId.h"
#include "domain/TurnoutPosition.h"
#include "domain/TurnoutState.h"
#include "domain/Orientation.h"
#include "domain/FeedbackSensor.h"
#include "domain/TurnoutMotion.h"
#include "domain/Duration.h"
#include "domain/Instant.h"
#include "domain/Level.h"
#include "ports/TurnoutCommandSink.h"
#include "ports/DigitalOutput.h"
#include "ports/DigitalInput.h"
#include "ports/PositionReporter.h"
#include "support/FakeDigitalOutput.h"
#include "support/FakeDigitalInput.h"
#include "support/FakePositionReporter.h"

namespace
{
Turnout makeTurnout(int id, DigitalOutput& output, DigitalInput& input, PositionReporter& reporter)
{
    Orientation orientation = Orientation::normal();
    FeedbackSensor sensor(input, orientation, Duration(50));
    TurnoutMotion motion(TurnoutPosition::closed(), Duration(200), Duration(50));
    return Turnout(TurnoutId(id), output, orientation, std::move(sensor), std::move(motion), reporter);
}
}

TEST_CASE("A buffered command is applied to the right turnout on tick, by channel arithmetic")
{
    std::array<FakeDigitalOutput, 8> outputs;
    std::array<FakeDigitalInput, 8> inputs;
    FakePositionReporter reporter;
    std::array<Turnout, 8> turnouts{
        makeTurnout(101, outputs[0], inputs[0], reporter),
        makeTurnout(102, outputs[1], inputs[1], reporter),
        makeTurnout(103, outputs[2], inputs[2], reporter),
        makeTurnout(104, outputs[3], inputs[3], reporter),
        makeTurnout(105, outputs[4], inputs[4], reporter),
        makeTurnout(106, outputs[5], inputs[5], reporter),
        makeTurnout(107, outputs[6], inputs[6], reporter),
        makeTurnout(108, outputs[7], inputs[7], reporter)
    };
    TurnoutRegistry registry(1, std::move(turnouts));

    registry.command(TurnoutId(103), TurnoutPosition::thrown());
    registry.tick(Instant(0));

    REQUIRE(outputs[2].level() == Level::High);
    REQUIRE(outputs[3].level() == Level::Low);
}

TEST_CASE("A command for a different node's id is dropped")
{
    std::array<FakeDigitalOutput, 8> outputs;
    std::array<FakeDigitalInput, 8> inputs;
    FakePositionReporter reporter;
    std::array<Turnout, 8> turnouts{
        makeTurnout(101, outputs[0], inputs[0], reporter),
        makeTurnout(102, outputs[1], inputs[1], reporter),
        makeTurnout(103, outputs[2], inputs[2], reporter),
        makeTurnout(104, outputs[3], inputs[3], reporter),
        makeTurnout(105, outputs[4], inputs[4], reporter),
        makeTurnout(106, outputs[5], inputs[5], reporter),
        makeTurnout(107, outputs[6], inputs[6], reporter),
        makeTurnout(108, outputs[7], inputs[7], reporter)
    };
    TurnoutRegistry registry(1, std::move(turnouts));

    registry.command(TurnoutId(201), TurnoutPosition::thrown());
    registry.tick(Instant(0));

    REQUIRE(outputs[0].level() == Level::Low);
}

TEST_CASE("A command with an out-of-range channel is dropped")
{
    std::array<FakeDigitalOutput, 8> outputs;
    std::array<FakeDigitalInput, 8> inputs;
    FakePositionReporter reporter;
    std::array<Turnout, 8> turnouts{
        makeTurnout(101, outputs[0], inputs[0], reporter),
        makeTurnout(102, outputs[1], inputs[1], reporter),
        makeTurnout(103, outputs[2], inputs[2], reporter),
        makeTurnout(104, outputs[3], inputs[3], reporter),
        makeTurnout(105, outputs[4], inputs[4], reporter),
        makeTurnout(106, outputs[5], inputs[5], reporter),
        makeTurnout(107, outputs[6], inputs[6], reporter),
        makeTurnout(108, outputs[7], inputs[7], reporter)
    };
    TurnoutRegistry registry(1, std::move(turnouts));

    registry.command(TurnoutId(109), TurnoutPosition::thrown());
    registry.tick(Instant(0));

    for (const auto& output : outputs)
    {
        REQUIRE(output.level() == Level::Low);
    }
}

TEST_CASE("A newer command overwrites a still-pending slot before tick (retargeting)")
{
    std::array<FakeDigitalOutput, 8> outputs;
    std::array<FakeDigitalInput, 8> inputs;
    FakePositionReporter reporter;
    std::array<Turnout, 8> turnouts{
        makeTurnout(101, outputs[0], inputs[0], reporter),
        makeTurnout(102, outputs[1], inputs[1], reporter),
        makeTurnout(103, outputs[2], inputs[2], reporter),
        makeTurnout(104, outputs[3], inputs[3], reporter),
        makeTurnout(105, outputs[4], inputs[4], reporter),
        makeTurnout(106, outputs[5], inputs[5], reporter),
        makeTurnout(107, outputs[6], inputs[6], reporter),
        makeTurnout(108, outputs[7], inputs[7], reporter)
    };
    TurnoutRegistry registry(1, std::move(turnouts));

    registry.command(TurnoutId(104), TurnoutPosition::thrown());
    registry.command(TurnoutId(104), TurnoutPosition::closed());
    registry.tick(Instant(0));

    REQUIRE(outputs[3].level() == Level::Low);
}

TEST_CASE("tick() with no pending commands still fans out and reports every turnout's initial state")
{
    std::array<FakeDigitalOutput, 8> outputs;
    std::array<FakeDigitalInput, 8> inputs;
    FakePositionReporter reporter;
    std::array<Turnout, 8> turnouts{
        makeTurnout(101, outputs[0], inputs[0], reporter),
        makeTurnout(102, outputs[1], inputs[1], reporter),
        makeTurnout(103, outputs[2], inputs[2], reporter),
        makeTurnout(104, outputs[3], inputs[3], reporter),
        makeTurnout(105, outputs[4], inputs[4], reporter),
        makeTurnout(106, outputs[5], inputs[5], reporter),
        makeTurnout(107, outputs[6], inputs[6], reporter),
        makeTurnout(108, outputs[7], inputs[7], reporter)
    };
    TurnoutRegistry registry(1, std::move(turnouts));

    registry.tick(Instant(0));

    REQUIRE(reporter.reports().size() == 8);
    REQUIRE(reporter.reports()[0].id == TurnoutId(101));
    REQUIRE(reporter.reports()[0].state == TurnoutState::Closed);
    REQUIRE(reporter.reports()[7].id == TurnoutId(108));
    REQUIRE(reporter.reports()[7].state == TurnoutState::Closed);
}

TEST_CASE("TurnoutRegistry satisfies TurnoutCommandSink through a base-class reference")
{
    std::array<FakeDigitalOutput, 8> outputs;
    std::array<FakeDigitalInput, 8> inputs;
    FakePositionReporter reporter;
    std::array<Turnout, 8> turnouts{
        makeTurnout(101, outputs[0], inputs[0], reporter),
        makeTurnout(102, outputs[1], inputs[1], reporter),
        makeTurnout(103, outputs[2], inputs[2], reporter),
        makeTurnout(104, outputs[3], inputs[3], reporter),
        makeTurnout(105, outputs[4], inputs[4], reporter),
        makeTurnout(106, outputs[5], inputs[5], reporter),
        makeTurnout(107, outputs[6], inputs[6], reporter),
        makeTurnout(108, outputs[7], inputs[7], reporter)
    };
    TurnoutRegistry registry(1, std::move(turnouts));
    TurnoutCommandSink& sink = registry;

    sink.command(TurnoutId(105), TurnoutPosition::thrown());
    registry.tick(Instant(0));

    REQUIRE(outputs[4].level() == Level::High);
}
