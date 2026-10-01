#pragma once

#include "DataTypes.h"

#include <chrono>
#include <optional>
#include <string>

namespace ProductionHMI::Core {

struct AlarmConfig {
    bool enabled{false};
    double highLimit{0.0};
    double highRecover{0.0};
    double lowLimit{0.0};
    double lowRecover{0.0};
    uint32_t delayMs{0};
};

struct WriteRange {
    double minimum{0.0};
    double maximum{0.0};
};

struct TagDefinition {
    TagId id;
    std::string name;

    RegisterType registerType{RegisterType::HoldingRegister};
    uint16_t address{0};

    DataType dataType{DataType::UInt16};
    ByteOrder byteOrder{ByteOrder::ABCD};

    double scale{1.0};
    double offset{0.0};

    std::string unit;

    bool writable{false};
    uint32_t scanPeriodMs{1000};
    std::optional<WriteRange> writeRange;
    std::optional<AlarmConfig> alarm;
};

struct TagValue {
    TagId id;
    RawValue value;
    Quality quality{Quality::Unknown};
    std::chrono::system_clock::time_point timestamp;
};

}
