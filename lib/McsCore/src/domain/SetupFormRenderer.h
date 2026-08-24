#pragma once

#include <array>
#include <cctype>
#include <string>

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
};
