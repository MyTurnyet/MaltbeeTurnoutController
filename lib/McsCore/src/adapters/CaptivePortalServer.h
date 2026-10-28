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
#include "domain/SetupFormRenderer.h"

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
        webServer_.send(200, "text/html", SetupFormRenderer::render(adapter_.currentValues()).c_str());
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
        std::string brokerHostOctet1 = webServer_.arg("broker_host_1").c_str();
        std::string brokerHostOctet2 = webServer_.arg("broker_host_2").c_str();
        std::string brokerHostOctet3 = webServer_.arg("broker_host_3").c_str();
        std::string brokerHostOctet4 = webServer_.arg("broker_host_4").c_str();
        form.brokerHost = brokerHostOctet1 + "." + brokerHostOctet2 + "." + brokerHostOctet3 + "." + brokerHostOctet4;
        form.brokerPort = webServer_.arg("broker_port").c_str();

        for (size_t i = 0; i < form.turnouts.size(); ++i)
        {
            std::string prefix = "t" + std::to_string(i + 1) + "_";
            form.turnouts[i].pin = webServer_.arg((prefix + "pin").c_str()).c_str();
            form.turnouts[i].feedbackPin = webServer_.arg((prefix + "fb").c_str()).c_str();
            form.turnouts[i].orientation = webServer_.arg((prefix + "orientation").c_str()).c_str();
            form.turnouts[i].settleMs = webServer_.arg((prefix + "settle").c_str()).c_str();
            form.turnouts[i].timeoutMs = webServer_.arg((prefix + "timeout").c_str()).c_str();
            form.turnouts[i].locoNetAddress = webServer_.arg((prefix + "addr").c_str()).c_str();
        }

        return form;
    }

    WebFormCommissioningAdapter& adapter_;
    MacAddress apMac_;
    DNSServer dnsServer_;
    WebServer webServer_;
};

#endif
