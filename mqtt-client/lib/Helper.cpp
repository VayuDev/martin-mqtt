#include "Helper.hpp"

#ifdef __unix__
#include <arpa/inet.h>
#elif WIN32
#include "WindowsInclude.hpp"
#endif
#include <cstring>

namespace martin {

void appendToBuffer(std::vector<uint8_t>& buffer, const std::string_view& str) {
    buffer.resize(buffer.size() + str.size());
    memcpy(buffer.data() + buffer.size() - str.size(), str.data(), str.size());
}
void appendVarIntToBuffer(std::vector<uint8_t>& buffer, uint32_t x) {
    do {
        uint8_t encodeByte = x % 128;
        x = x / 128;
        // if there are more bytes to encode, set the upper bit
        if(x > 0) {
            encodeByte |= 128;
        }
        buffer.emplace_back(encodeByte);
    } while(x > 0);
}
void appendUint16_tToBuffer(std::vector<uint8_t>& buffer, uint16_t val) {
    auto encoded = htons(val);
    buffer.resize(buffer.size() + 2);
    memcpy(buffer.data() + buffer.size() - 2, &encoded, 2);
}
void appendString2ByteLengthToBuffer(std::vector<uint8_t>& buffer, const std::string_view& str) {
    appendUint16_tToBuffer(buffer, str.size());
    appendToBuffer(buffer, str);
}
void insertMessageLengthToHeader(std::vector<uint8_t>& buffer) {
    std::vector<uint8_t> lenBuffer;
    appendVarIntToBuffer(lenBuffer, buffer.size() - 1);
    buffer.insert(buffer.begin() + 1, lenBuffer.begin(), lenBuffer.end());
}
std::string extractStringFromBuffer(const std::vector<uint8_t>& buffer, size_t& offset) {
    uint16_t length;
    memcpy(&length, buffer.data() + offset, 2);
    length = ntohs(length);
    std::string ret;
    ret.reserve(length);
    for(size_t i = offset + 2; i < offset + length + 2; ++i) {
        ret.push_back(buffer.at(i));
    }
    offset += length + 2;
    return ret;
}
std::vector<std::string> splitStringBySlash(const std::string& src) {
    std::vector<std::string> ret;
    size_t offset = 0;
    while(true) {
        auto index = src.find('/', offset);
        if(index == offset) {
            offset++;
            continue;
        }
        ret.push_back(src.substr(offset, index - offset));
        if(index == std::string::npos) {
            break;
        }
        offset = index + 1;
    }
    return ret;
}
bool doesTopicMatchPattern(const std::string& topic, const std::string& pattern) {
    if(topic == pattern)
        return true;
    auto topicSplit = splitStringBySlash(topic);
    auto patternSplit = splitStringBySlash(pattern);
    for(size_t i = 0; i < topicSplit.size(); ++i) {
        if(i >= patternSplit.size()) {
            return false;
        }
        if(topicSplit.at(i) != patternSplit.at(i)) {
            if(patternSplit.at(i) == "+") {
                continue;
            } else if(patternSplit.at(i) == "#") {
                return true;
            } else {
                return false;
            }
        }
    }

    return true;
}

}