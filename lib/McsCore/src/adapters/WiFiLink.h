#pragma once

#ifdef ARDUINO

#include <Arduino.h>
#include <WiFi.h>

#include <string>

#include "ports/Clock.h"
#include "domain/Instant.h"
#include "domain/Duration.h"
#include "domain/WifiCredentials.h"

class WiFiLink
{
public:
    WiFiLink(Clock& clock, Duration retryInterval)
        : clock_(clock), retryInterval_(retryInterval), lastAttempt_(Instant(0))
    {
    }

    void begin(const WifiCredentials& credentials)
    {
        ssid_ = credentials.ssid();
        password_ = credentials.password();

        // A prior wireless-setup session (CaptivePortalServer) calls
        // WiFi.softAP(), and esp_wifi persists mode/config to flash by
        // default - so a leftover AP interface (and the channel it started
        // on) can still be active on every later boot, including this one.
        // ESP32 can only run STA+AP concurrently on a single shared radio
        // channel, so an AP left on a different channel than the router can
        // block station association outright. Force STA-only before the
        // first connect attempt.
        WiFi.mode(WIFI_STA);

        connect();
    }

    void poll()
    {
        logStatusChange();

        if (WiFi.status() == WL_CONNECTED)
        {
            return;
        }

        if (clock_.now() - lastAttempt_ >= retryInterval_)
        {
            connect();
        }
    }

    bool connected() const
    {
        return WiFi.status() == WL_CONNECTED;
    }

private:
    void connect()
    {
        WiFi.begin(ssid_.c_str(), password_.c_str());
        lastAttempt_ = clock_.now();
    }

    // Logs only on status transitions (not every poll) so a stuck retry
    // loop doesn't flood the serial monitor - gives visibility into
    // association failures (wrong SSID/password, no route after connect)
    // that were previously silent.
    void logStatusChange()
    {
        wl_status_t status = WiFi.status();
        if (status == lastLoggedStatus_)
        {
            return;
        }
        lastLoggedStatus_ = status;
        Serial.printf("[WiFiLink] status=%d ip=%s rssi=%d\n",
                      status, WiFi.localIP().toString().c_str(), WiFi.RSSI());
    }

    Clock& clock_;
    Duration retryInterval_;
    Instant lastAttempt_;
    std::string ssid_;
    std::string password_;
    int lastLoggedStatus_ = -1;
};

#endif
