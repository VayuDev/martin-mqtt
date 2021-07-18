#include "MQTTConfigBuilder.hpp"

namespace martin {

MQTTConfigBuilder& MQTTConfigBuilder::will(MQTTMessage willMsg) {
    mConfig.mWillMsg = std::move(willMsg);
    return *this;
}
MQTTConfigBuilder& MQTTConfigBuilder::hostname(std::string hostname) {
    mConfig.mHostname = std::move(hostname);
    return *this;
}
MQTTConfigBuilder& MQTTConfigBuilder::port(uint16_t port) {
    mConfig.mPort = port;
    return *this;
}
MQTTConfigBuilder& MQTTConfigBuilder::username(std::string username) {
    mConfig.mUsername = std::move(username);
    return *this;
}
MQTTConfigBuilder& MQTTConfigBuilder::password(std::string password) {
    mConfig.mPassword = std::move(password);
    return *this;
}
MQTTConfigBuilder& MQTTConfigBuilder::clientID(std::string clientID) {
    mConfig.mClientID = std::move(clientID);
    return *this;
}
MQTTConfigBuilder& MQTTConfigBuilder::cleanSession(CleanSession cleanSession) {
    mConfig.mCleanSession = cleanSession;
    return *this;
}
MQTTConfigBuilder& MQTTConfigBuilder::keepAliveInterval(uint16_t keepAliveInterval) {
    mConfig.mKeepAliveIntervalSeconds = keepAliveInterval;
    return *this;
}
[[maybe_unused]] MQTTConfigBuilder& MQTTConfigBuilder::delayBetweenRetries(std::chrono::milliseconds delayBetweenRetries) {
    mConfig.mDelayBetweenRetries = delayBetweenRetries;
    return *this;
}
MQTTConfig MQTTConfigBuilder::build() const {
    return mConfig;
}
MQTTConfigBuilder& MQTTConfigBuilder::maxRetriesOnDisconnect(int retries) {
    mConfig.mMaxRetries = retries;
    return *this;
}

}