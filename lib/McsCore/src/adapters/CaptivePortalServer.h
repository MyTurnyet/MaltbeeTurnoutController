// lib/McsCore/src/adapters/CaptivePortalServer.h
#pragma once

#ifdef ARDUINO

#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include <string>

#include "domain/MacAddress.h"
#include "domain/SetupApName.h"
#include "domain/WebFormSubmission.h"
#include "adapters/WebFormCommissioningAdapter.h"

class CaptivePortalServer
{
public:
    CaptivePortalServer(WebFormCommissioningAdapter& adapter, MacAddress apMac)
        : adapter_(adapter), apMac_(apMac), webServer_(80)
    {
    }

    void begin()
    {
        std::string apName = SetupApName::from(apMac_);
        WiFi.softAP(apName.c_str());

        IPAddress apIp = WiFi.softAPIP();
        dnsServer_.start(53, "*", apIp);

        webServer_.on("/", [this]() { handleRoot(); });
        webServer_.on("/submit", HTTP_POST, [this]() { handleSubmit(); });
        webServer_.onNotFound([this]() { handleRoot(); });
        webServer_.begin();
    }

    // Non-blocking - call repeatedly from a loop. No delay().
    void poll()
    {
        dnsServer_.processNextRequest();
        webServer_.handleClient();
    }

private:
    void handleRoot()
    {
        webServer_.send(200, "text/html", kFormHtml);
    }

    void handleSubmit()
    {
        WebFormSubmission form = readForm();
        std::string response = adapter_.submit(form);
        webServer_.send(200, "text/plain", response.c_str());
    }

    WebFormSubmission readForm()
    {
        WebFormSubmission form;
        form.nodeId = webServer_.arg("id").c_str();
        form.wifiSsid = webServer_.arg("wifi_ssid").c_str();
        form.wifiPassword = webServer_.arg("wifi_password").c_str();
        form.brokerHost = webServer_.arg("broker_host").c_str();
        form.brokerPort = webServer_.arg("broker_port").c_str();

        for (size_t i = 0; i < form.turnouts.size(); ++i)
        {
            std::string prefix = "t" + std::to_string(i + 1) + "_";
            form.turnouts[i].pin = webServer_.arg((prefix + "pin").c_str()).c_str();
            form.turnouts[i].feedbackPin = webServer_.arg((prefix + "fb").c_str()).c_str();
            form.turnouts[i].orientation = webServer_.arg((prefix + "orientation").c_str()).c_str();
            form.turnouts[i].settleMs = webServer_.arg((prefix + "settle").c_str()).c_str();
            form.turnouts[i].timeoutMs = webServer_.arg((prefix + "timeout").c_str()).c_str();
        }

        return form;
    }

    static constexpr const char* kFormHtml =
        "<html><body><h1>Tortoise Setup</h1>"
        "<form method='POST' action='/submit'>"
        "Node ID: <input name='id'><br>"
        "WiFi SSID: <input name='wifi_ssid'><br>"
        "WiFi Password: <input name='wifi_password' type='password'><br>"
        "Broker Host: <input name='broker_host'><br>"
        "Broker Port: <input name='broker_port'><br>"
        "<input type='submit' value='Save'>"
        "</form></body></html>";

    WebFormCommissioningAdapter& adapter_;
    MacAddress apMac_;
    DNSServer dnsServer_;
    WebServer webServer_;
};

#endif
