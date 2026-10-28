#pragma once

#include <array>
#include <cctype>
#include <string>
#include <vector>

#include "domain/WebFormSubmission.h"

class SetupFormRenderer
{
public:
    // Splits a dotted-quad IPv4 string into its 4 octets. Returns
    // {"", "", "", ""} unless the string is exactly 4 dot-separated numeric
    // parts, each in 0-255 - used to pre-fill the setup form's broker-host
    // boxes without ever showing a misleading default for an unset or
    // non-IPv4 (e.g. hostname) saved value.
    static std::array<std::string, 4> splitIpv4Octets(const std::string& host)
    {
        std::array<std::string, 4> empty{"", "", "", ""};
        std::array<std::string, 4> parts;
        size_t partIndex = 0;
        std::string current;

        for (char c : host)
        {
            if (c == '.')
            {
                if (partIndex >= 4)
                {
                    return empty;
                }
                parts[partIndex++] = current;
                current.clear();
            }
            else
            {
                current += c;
            }
        }

        if (partIndex != 3)
        {
            return empty;
        }
        parts[partIndex++] = current;

        for (const std::string& part : parts)
        {
            if (part.empty() || part.size() > 3)
            {
                return empty;
            }
            for (char c : part)
            {
                if (!std::isdigit(static_cast<unsigned char>(c)))
                {
                    return empty;
                }
            }
            if (std::stoi(part) > 255)
            {
                return empty;
            }
        }

        return parts;
    }

    // A saved SSID/password/broker value could contain HTML-significant
    // characters - escape before embedding in an attribute value so it
    // can't break the page or inject markup back into the same form.
    static std::string escapeHtml(const std::string& text)
    {
        std::string escaped;
        escaped.reserve(text.size());
        for (char c : text)
        {
            switch (c)
            {
            case '&':
                escaped += "&amp;";
                break;
            case '<':
                escaped += "&lt;";
                break;
            case '>':
                escaped += "&gt;";
                break;
            case '"':
                escaped += "&quot;";
                break;
            case '\'':
                escaped += "&#39;";
                break;
            default:
                escaped += c;
            }
        }
        return escaped;
    }

    static std::string render(const WebFormSubmission& values)
    {
        std::array<std::string, 4> octets = splitIpv4Octets(values.brokerHost);

        std::string html;
        html += "<!DOCTYPE html><html><head>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
        html += "<style>" + kStyle + "</style></head><body>";
        html += "<div class='card'><h1>Tortoise Setup</h1>";
        html += "<p class='subtitle'>Configure this board's network settings</p>";
        html += "<form method='POST' action='/submit'>";
        html += "<label>Node ID</label><select name='id'>" + renderIdOptions(values.nodeId) + "</select>";
        html += "<label>WiFi SSID</label><input name='wifi_ssid' value='" + escapeHtml(values.wifiSsid) + "'>";
        html += "<label>WiFi Password</label><input name='wifi_password' type='password' value='"
            + escapeHtml(values.wifiPassword) + "'>";
        html += "<label>Broker Host</label><div class='octets'>";
        for (size_t i = 0; i < octets.size(); ++i)
        {
            if (i > 0)
            {
                html += "<span>.</span>";
            }
            html += "<input name='broker_host_" + std::to_string(i + 1)
                + "' type='number' min='0' max='255' value='" + escapeHtml(octets[i]) + "'>";
        }
        html += "</div>";
        html += "<label>Broker Port</label><input name='broker_port' type='number' value='"
            + escapeHtml(values.brokerPort) + "'>";
        html += "<details><summary>Advanced: Turnout Wiring</summary>";
        html += "<p class='warning'>Saving this form applies pin assignments to every "
            "channel below, including ones you don't change here.</p>";
        for (size_t i = 0; i < values.turnouts.size(); ++i)
        {
            html += renderTurnoutChannel(static_cast<int>(i) + 1, values.turnouts[i]);
        }
        html += "</details>";
        html += "<button type='submit'>Save</button>";
        html += "</form></div></body></html>";
        return html;
    }

private:
    static std::string renderIdOptions(const std::string& selectedId)
    {
        std::string options;
        if (selectedId.empty())
        {
            options += "<option value='' disabled selected hidden>-- select --</option>";
        }
        for (int i = 1; i <= 16; ++i)
        {
            std::string value = std::to_string(i);
            options += "<option value='" + value + "'" + (value == selectedId ? " selected" : "") + ">" + value + "</option>";
        }
        return options;
    }

    static std::string renderPinOptions(const std::vector<int>& options, const std::string& selectedValue)
    {
        std::string html;
        html += "<option value=''" + std::string(selectedValue.empty() ? " selected" : "") + ">-- not wired --</option>";
        for (int pin : options)
        {
            std::string value = std::to_string(pin);
            html += "<option value='" + value + "'" + (value == selectedValue ? " selected" : "") + ">" + value + "</option>";
        }
        return html;
    }

    static std::string renderOrientationOptions(const std::string& selectedOrientation)
    {
        std::string effective = selectedOrientation.empty() ? "normal" : selectedOrientation;
        std::string html;
        html += "<option value='normal'" + std::string(effective == "normal" ? " selected" : "") + ">normal</option>";
        html += "<option value='inverted'" + std::string(effective == "inverted" ? " selected" : "") + ">inverted</option>";
        return html;
    }

    static std::string renderTurnoutChannel(int channelNumber, const WebFormTurnoutField& field)
    {
        int index = channelNumber - 1;
        std::string defaultPin = std::to_string(kDefaultPin[index]);
        std::string defaultFeedbackPin = std::to_string(kDefaultFeedbackPin[index]);
        std::string selectedPin = field.pin.empty() ? defaultPin : field.pin;
        std::string selectedFeedbackPin = field.feedbackPin.empty() ? defaultFeedbackPin : field.feedbackPin;
        std::string settleValue = field.settleMs.empty() ? "500" : field.settleMs;
        std::string timeoutValue = field.timeoutMs.empty() ? "4000" : field.timeoutMs;
        std::string prefix = "t" + std::to_string(channelNumber) + "_";

        std::string html;
        html += "<fieldset><legend>Turnout " + std::to_string(channelNumber) + "</legend>";
        html += "<label>Pin</label><select name='" + prefix + "pin'>"
            + renderPinOptions(kOutputPinOptions, selectedPin) + "</select>";
        html += "<label>Feedback Pin</label><select name='" + prefix + "fb'>"
            + renderPinOptions(kFeedbackPinOptions, selectedFeedbackPin) + "</select>";
        html += "<label>Orientation</label><select name='" + prefix + "orientation'>"
            + renderOrientationOptions(field.orientation) + "</select>";
        html += "<label>Settle (ms)</label><input name='" + prefix + "settle' type='number' value='"
            + escapeHtml(settleValue) + "'>";
        html += "<label>Timeout (ms)</label><input name='" + prefix + "timeout' type='number' value='"
            + escapeHtml(timeoutValue) + "'>";
        html += "<label>LocoNet Address</label><input name='" + prefix + "addr' type='number' min='0' max='2048' value='"
            + escapeHtml(field.locoNetAddress) + "'>";
        html += "</fieldset>";
        return html;
    }

    static inline const std::vector<int> kOutputPinOptions{4, 13, 14, 16, 17, 18, 19, 25, 26, 27, 32, 33};
    static inline const std::vector<int> kFeedbackPinOptions{4, 13, 14, 16, 17, 18, 19, 25, 26, 27, 32, 33, 34, 35, 36, 39};
    static inline const std::array<int, 8> kDefaultPin{13, 14, 27, 26, 25, 33, 32, 4};
    static inline const std::array<int, 8> kDefaultFeedbackPin{36, 39, 34, 35, 16, 17, 18, 19};

    static inline const std::string kStyle =
        "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;"
        "background:#f2f4f7;margin:0;padding:24px 16px;color:#1f2933;}"
        ".card{max-width:420px;margin:0 auto;background:#fff;border-radius:12px;"
        "box-shadow:0 1px 3px rgba(0,0,0,0.1);padding:24px;}"
        "h1{font-size:1.25rem;margin:0 0 4px;}"
        ".subtitle{color:#6b7280;font-size:0.875rem;margin:0 0 20px;}"
        "label{display:block;font-size:0.8rem;font-weight:600;color:#374151;margin:16px 0 4px;}"
        "input,select{width:100%;box-sizing:border-box;padding:8px 10px;border:1px solid #d1d5db;"
        "border-radius:6px;font-size:0.95rem;}"
        ".octets{display:flex;align-items:center;gap:6px;}"
        ".octets input{width:4rem;text-align:center;flex:none;}"
        ".octets span{color:#9ca3af;}"
        "button{margin-top:24px;width:100%;padding:10px;background:#2563eb;color:#fff;"
        "border:none;border-radius:6px;font-size:1rem;font-weight:600;cursor:pointer;}"
        "button:hover{background:#1d4ed8;}"
        "details{margin-top:20px;}"
        "summary{cursor:pointer;font-size:0.85rem;font-weight:600;color:#374151;}"
        "fieldset{border:1px solid #e5e7eb;border-radius:8px;margin-top:12px;padding:8px 12px 12px;}"
        "legend{font-size:0.8rem;font-weight:600;color:#374151;padding:0 4px;}"
        ".warning{color:#b45309;font-size:0.8rem;margin:8px 0 0;}";
};
