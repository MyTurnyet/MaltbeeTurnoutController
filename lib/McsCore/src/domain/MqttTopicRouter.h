#pragma once

#include <algorithm>
#include <functional>
#include <string>
#include <utility>
#include <vector>

class MqttTopicRouter
{
public:
    using Handler = std::function<void(const std::string&)>;

    void on(const std::string& topic, Handler handler)
    {
        handlers_.emplace_back(topic, std::move(handler));
    }

    void dispatch(const std::string& topic, const std::string& payload) const
    {
        for (const auto& entry : handlers_)
        {
            if (entry.first == topic)
            {
                entry.second(payload);
            }
        }
    }

    // Every topic ever registered via on(), deduplicated. Lets an adapter
    // (e.g. MqttLink) replay subscriptions after a reconnect without keeping
    // its own separate list.
    std::vector<std::string> topics() const
    {
        std::vector<std::string> result;
        for (const auto& entry : handlers_)
        {
            if (std::find(result.begin(), result.end(), entry.first) == result.end())
            {
                result.push_back(entry.first);
            }
        }
        return result;
    }

private:
    std::vector<std::pair<std::string, Handler>> handlers_;
};
