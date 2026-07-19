#pragma once
#include <cstdint>

namespace alicloud::iot {

/// Generates monotonically increasing Alink protocol message IDs.
class MessageIdGenerator {
public:
    static uint32_t generate() { return ++counter_; }
private:
    static uint32_t counter_;
};

} // namespace alicloud::iot
