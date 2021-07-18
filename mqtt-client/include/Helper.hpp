#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>
#include <string>
#include "WindowsInclude.hpp"

namespace martin {

void appendToBuffer(std::vector<uint8_t>& buffer, const std::string_view& str);

void appendVarIntToBuffer(std::vector<uint8_t>& buffer, uint32_t val);

void appendUint16_tToBuffer(std::vector<uint8_t>& buffer, uint16_t val);

void appendString2ByteLengthToBuffer(std::vector<uint8_t>& buffer, const std::string_view& str);

void insertMessageLengthToHeader(std::vector<uint8_t>& buffer);

std::string extractStringFromBuffer(const std::vector<uint8_t>& buffer, size_t& offset);

class DestructWrapper final {
public:
    explicit DestructWrapper(std::function<void()>&& callback) : mCallback(std::move(callback)) {
    }

    ~DestructWrapper() {
        if(mCallback) {
            mCallback.value()();
            mCallback.reset();
        }
    }

    DestructWrapper(const DestructWrapper&) = delete;

    void operator=(const DestructWrapper&) = delete;

    DestructWrapper(DestructWrapper&&) = delete;

    void operator=(DestructWrapper&&) = delete;

    void disarm() {
        mCallback.reset();
    }

private:
    std::optional<std::function<void()>> mCallback;
};

std::vector<std::string> splitStringBySlash(const std::string& src);

bool doesTopicMatchPattern(const std::string& topic, const std::string& pattern);

}
