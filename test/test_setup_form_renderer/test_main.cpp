#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>

#include "domain/SetupFormRenderer.h"

namespace
{
std::string extractSelect(const std::string& html, const std::string& selectName)
{
    std::string marker = "name='" + selectName + "'>";
    size_t start = html.find(marker);
    if (start == std::string::npos)
    {
        return "";
    }
    size_t end = html.find("</select>", start);
    return html.substr(start, end - start);
}
}


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

TEST_CASE("render includes a collapsed Advanced Turnout Wiring section")
{
    WebFormSubmission values;
    values.brokerPort = "1883";

    std::string html = SetupFormRenderer::render(values);

    REQUIRE(html.find("<details>") != std::string::npos);
    REQUIRE(html.find("<summary>Advanced: Turnout Wiring</summary>") != std::string::npos);
}

TEST_CASE("render defaults each channel's pin dropdown to its documented GPIO when unset")
{
    WebFormSubmission values;
    values.brokerPort = "1883";

    std::string html = SetupFormRenderer::render(values);

    REQUIRE(extractSelect(html, "t1_pin").find("<option value='13' selected>13</option>") != std::string::npos);
    REQUIRE(extractSelect(html, "t8_pin").find("<option value='4' selected>4</option>") != std::string::npos);
}

TEST_CASE("render defaults each channel's feedback pin dropdown to its documented GPIO when unset")
{
    WebFormSubmission values;
    values.brokerPort = "1883";

    std::string html = SetupFormRenderer::render(values);

    REQUIRE(extractSelect(html, "t1_fb").find("<option value='36' selected>36</option>") != std::string::npos);
    REQUIRE(extractSelect(html, "t8_fb").find("<option value='19' selected>19</option>") != std::string::npos);
}

TEST_CASE("render selects a saved pin over the documented default")
{
    WebFormSubmission values;
    values.brokerPort = "1883";
    values.turnouts[0].pin = "19";

    std::string html = SetupFormRenderer::render(values);

    std::string pinSelect = extractSelect(html, "t1_pin");
    REQUIRE(pinSelect.find("<option value='19' selected>19</option>") != std::string::npos);
    REQUIRE(pinSelect.find("<option value='13' selected>13</option>") == std::string::npos);
}

TEST_CASE("render defaults orientation to normal when unset and reflects a saved inverted value")
{
    WebFormSubmission values;
    values.brokerPort = "1883";
    values.turnouts[1].orientation = "inverted";

    std::string html = SetupFormRenderer::render(values);

    REQUIRE(extractSelect(html, "t1_orientation").find("<option value='normal' selected>normal</option>") != std::string::npos);
    REQUIRE(extractSelect(html, "t2_orientation").find("<option value='inverted' selected>inverted</option>") != std::string::npos);
}

TEST_CASE("render defaults settle and timeout when unset and reflects saved values")
{
    WebFormSubmission values;
    values.brokerPort = "1883";
    values.turnouts[2].settleMs = "750";
    values.turnouts[2].timeoutMs = "6000";

    std::string html = SetupFormRenderer::render(values);

    REQUIRE(html.find("name='t1_settle' type='number' value='500'") != std::string::npos);
    REQUIRE(html.find("name='t1_timeout' type='number' value='4000'") != std::string::npos);
    REQUIRE(html.find("name='t3_settle' type='number' value='750'") != std::string::npos);
    REQUIRE(html.find("name='t3_timeout' type='number' value='6000'") != std::string::npos);
}
