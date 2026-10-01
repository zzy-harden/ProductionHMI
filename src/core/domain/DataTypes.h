#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <variant>

namespace ProductionHMI::Core {

using DeviceId = std::string;
using TagId = std::string;

enum class RegisterType {
    HoldingRegister,
    InputRegister
};

enum class DataType {
    UInt16,
    Int16,
    UInt32,
    Int32,
    Float32,
    Bit
};

enum class ByteOrder {
    ABCD,
    BADC,
    CDAB,
    DCBA
};

enum class Quality {
    Good,
    Timeout,
    Offline,
    Invalid,
    Unknown
};

enum class DeviceState {
    Stopped,
    Ready,
    Running,
    Completed,
    Fault,
    Offline
};

enum class AlarmType {
    HighLimit,
    LowLimit,
    Communication,
    DeviceFault
};

enum class AlarmState {
    Normal,
    Pending,
    ActiveUnack,
    ActiveAck,
    Recovered
};

enum class UserRole {
    Operator,
    Administrator
};

enum class ControlCommand {
    Start,
    Stop,
    Reset
};

using RawValue = std::variant<int16_t, uint16_t, int32_t, uint32_t, float, bool>;

}
