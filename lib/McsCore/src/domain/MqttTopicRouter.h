#pragma once

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

private:
    std::vector<std::pair<std::string, Handler>> handlers_;
};
