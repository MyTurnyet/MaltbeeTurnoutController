#pragma once

#include <cctype>
#include <optional>
#include <string>

#include "domain/TurnoutId.h"

class TopicScheme
{
public:
    static std::string topicFor(TurnoutId id)
    {
        return kPrefix + std::to_string(id.value());
    }

    static std::optional<TurnoutId> parse(const std::string& topic)
    {
        if (topic.rfind(kPrefix, 0) != 0)
        {
            return std::nullopt;
        }

        std::string suffix = topic.substr(kPrefix.size());

        if (suffix.empty())
        {
            return std::nullopt;
        }

        for (char c : suffix)
        {
            if (!std::isdigit(static_cast<unsigned char>(c)))
            {
                return std::nullopt;
            }
        }

        return TurnoutId(std::stoi(suffix));
    }

private:
    static inline const std::string kPrefix = "track/turnout/";
};
