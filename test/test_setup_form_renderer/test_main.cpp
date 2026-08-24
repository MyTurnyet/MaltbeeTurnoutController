#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>

#include "domain/SetupFormRenderer.h"

TEST_CASE("splitIpv4Octets splits a valid dotted quad")
{
    std::array<std::string, 4> octets = SetupFormRenderer::splitIpv4Octets("192.168.1.13");

    REQUIRE(octets[0] == "192");
    REQUIRE(octets[1] == "168");
    REQUIRE(octets[2] == "1");
    REQUIRE(octets[3] == "13");
}

TEST_CASE("splitIpv4Octets returns all-blank for an empty host")
{
    std::array<std::string, 4> octets = SetupFormRenderer::splitIpv4Octets("");

    REQUIRE(octets == std::array<std::string, 4>{"", "", "", ""});
}

TEST_CASE("splitIpv4Octets returns all-blank for a hostname")
{
    std::array<std::string, 4> octets = SetupFormRenderer::splitIpv4Octets("mqtt.local");

    REQUIRE(octets == std::array<std::string, 4>{"", "", "", ""});
}

TEST_CASE("splitIpv4Octets returns all-blank when an octet is out of range")
{
    std::array<std::string, 4> octets = SetupFormRenderer::splitIpv4Octets("192.168.1.999");

    REQUIRE(octets == std::array<std::string, 4>{"", "", "", ""});
}

TEST_CASE("splitIpv4Octets returns all-blank for the wrong number of parts")
{
    std::array<std::string, 4> octets = SetupFormRenderer::splitIpv4Octets("192.168.1");

    REQUIRE(octets == std::array<std::string, 4>{"", "", "", ""});
}

TEST_CASE("escapeHtml escapes ampersand, angle brackets, and quotes")
{
    std::string escaped = SetupFormRenderer::escapeHtml("Tom & Jerry's \"Layout\" <shed>");

    REQUIRE(escaped == "Tom &amp; Jerry&#39;s &quot;Layout&quot; &lt;shed&gt;");
}

TEST_CASE("escapeHtml leaves ordinary text unchanged")
{
    REQUIRE(SetupFormRenderer::escapeHtml("Layout Room") == "Layout Room");
}

TEST_CASE("render selects the placeholder when node id is empty")
{
    WebFormSubmission values;
    values.brokerPort = "1883";

    std::string html = SetupFormRenderer::render(values);

    REQUIRE(html.find("<option value='' disabled selected hidden>-- select --</option>") != std::string::npos);
    REQUIRE(html.find("<option value='5' selected>5</option>") == std::string::npos);
}

TEST_CASE("render marks the saved node id as selected")
{
    WebFormSubmission values;
    values.nodeId = "5";
    values.brokerPort = "1883";

    std::string html = SetupFormRenderer::render(values);

    REQUIRE(html.find("<option value='5' selected>5</option>") != std::string::npos);
    REQUIRE(html.find("disabled selected hidden") == std::string::npos);
}

TEST_CASE("render HTML-escapes the WiFi SSID and password")
{
    WebFormSubmission values;
    values.wifiSsid = "Tom & Jerry's";
    values.wifiPassword = "pa\"ss";
    values.brokerPort = "1883";

    std::string html = SetupFormRenderer::render(values);

    REQUIRE(html.find("name='wifi_ssid' value='Tom &amp; Jerry&#39;s'") != std::string::npos);
    REQUIRE(html.find("name='wifi_password' type='password' value='pa&quot;ss'") != std::string::npos);
}

TEST_CASE("render pre-fills the broker host octet boxes from a saved IP")
{
    WebFormSubmission values;
    values.brokerHost = "192.168.1.13";
    values.brokerPort = "1883";

    std::string html = SetupFormRenderer::render(values);

    REQUIRE(html.find("name='broker_host_1' type='number' min='0' max='255' value='192'") != std::string::npos);
    REQUIRE(html.find("name='broker_host_4' type='number' min='0' max='255' value='13'") != std::string::npos);
}

TEST_CASE("render leaves broker host octet boxes blank for an invalid saved host")
{
    WebFormSubmission values;
    values.brokerHost = "not-an-ip";
    values.brokerPort = "1883";

    std::string html = SetupFormRenderer::render(values);

    REQUIRE(html.find("name='broker_host_1' type='number' min='0' max='255' value=''") != std::string::npos);
}

TEST_CASE("render pre-fills the broker port")
{
    WebFormSubmission values;
    values.brokerPort = "1883";

    std::string html = SetupFormRenderer::render(values);

    REQUIRE(html.find("name='broker_port' type='number' value='1883'") != std::string::npos);
}
