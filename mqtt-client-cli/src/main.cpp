#include "MQTTClient.hpp"
#include "MQTTConfigBuilder.hpp"
#include <cstring>
#include <iostream>
#include <cassert>
#include "AbstractMQTTMessageHandler.hpp"
#include "../../thirdparty/cxxopts/cxxopts.h"

class PrintAllMessages : public martin::AbstractMQTTMessageHandler {
public:
    void handleMessage(const martin::MQTTMessage& msg) override {
        std::cout << msg.getTopic() << " - " << msg.getPayload() << "\n";
    }
};

int main(int argc, char** argv) {
    try {
        cxxopts::Options options("mqtt-client-cli", "A non-blocking MQTT implementation that can run singlethreaded");
        // clang-format off
        options.add_options()
            ("H, hostname", "Specify hostname/address of server", cxxopts::value<std::string>()->default_value("localhost"))
            ("P, port", "Specify Port of server", cxxopts::value<uint16_t>()->default_value("1883"))
            ("W, will", "Will Message", cxxopts::value<std::string>())
            ("T, willtopic", "Will topic on where to publish the will", cxxopts::value<std::string>())
            ("t, topic", "Topic to subscribe/publish to. Can be used multiple times. For Example: \"-t topic1 -t topic2\"", cxxopts::value<std::vector<std::string>>())
            ("s, subscribe", "Subscribe to topic. Subscribes to all topics passed as flags", cxxopts::value<bool>()->default_value("false"))
            ("p, publish", "Publishes to topic. Must be that same number of arguments as topic. Can be used multiple times. For Example \"-p msg1 -p msg2\"", cxxopts::value<std::vector<std::string>>())
            ("r, retain", R"(Retain flag for all publishes given with the "-p"-Flag. Also works with "-W")", cxxopts::value<bool>())
            ("h, help", "Help");
        // clang-format on

        auto result = options.parse(argc, argv);
        martin::MQTTConfigBuilder builder;
        builder.onConnect([] { std::cout << "Connected!\n"; });

        if(!result.count("H") && !result.count("P") && !result.count("W") && !result.count("h") && !result.count("r") && !result.count("T")
           && !result.count("t") && !result.count("s") && !result.count("p")) {
            std::cout << options.help() << std::endl;
            return 0;
        }

        if(result.count("h")) {
            std::cout << options.help() << std::endl;
            return 0;
        }
        if(result.count("H") || result["H"].has_default()) {
            builder.hostname(result["H"].as<std::string>());
        }
        if(result.count("P") || result["H"].has_default()) {
            builder.port(result["P"].as<uint16_t>());
        }
        if(result.count("W")) {
            auto flag = result["r"].as<bool>() ? martin::Retain::Yes : martin::Retain::No;
            if(!result.count("T") || !result.count("W")) {
                std::cout << options.help() << std::endl;
                return 0;
            }
            builder.will(martin::MQTTMessage{ result["T"].as<std::string>(), result["W"].as<std::string>(), flag });
        }

        martin::MQTTClient client{ builder.build() };

        if(result["s"].as<bool>()) {
            for(const auto& topic : result["t"].as<std::vector<std::string>>()) {
                client.subscribe(topic, martin::QoS::QoS0, PrintAllMessages());
            }
            while(true) {
                client.loop({}, martin::BlockForRecv::Yes);
            }
        } else if(result.count("p")) {
            auto publishMsg = result["p"].as<std::vector<std::string>>();
            if(!result.count("t")) {
                std::cout << options.help() << std::endl;
                return 0;
            }
            auto topics = result["t"].as<std::vector<std::string>>();
            if(publishMsg.size() != topics.size()) {
                std::cout << "Publish list and topic list have not the same number of arguments" << std::endl;
                return 0;
            }
            auto flag = result["r"].as<bool>() ? martin::Retain::Yes : martin::Retain::No;
            std::cout << static_cast<uint32_t>(flag) << std::endl;
            for(size_t i = 0; i < topics.size(); i++) {
                client.publish(martin::MQTTMessage{ topics[i], publishMsg[i], flag, martin::QoS::QoS0 });
            }
            client.disconnect();
            while(client.loop({}, martin::BlockForRecv::Yes) != martin::MQTTClientLoopStatus::EVERYTHING_DONE)
                ;
        } else {
            std::cout << "You need to specify -s or -p!\n";
        }

    } catch(std::exception& e) {
        std::cout << "Critical error: " << e.what() << std::endl;
    }
    return 0;
}
