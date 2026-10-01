#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ProductionHMI::Communication {

enum class ModbusEncodeError {
    None,
    InvalidQuantity,
    AddressRangeOverflow
};

struct ModbusEncodeResult {
    bool success{false};
    std::vector<uint8_t> bytes;
    ModbusEncodeError error{ModbusEncodeError::None};
    std::string message;
};

class ModbusCodec {
public:
    static ModbusEncodeResult encodeReadHoldingRegisters(
        uint16_t transactionId,
        uint8_t unitId,
        uint16_t startAddress,
        uint16_t quantity);

    static ModbusEncodeResult encodeReadInputRegisters(
        uint16_t transactionId,
        uint8_t unitId,
        uint16_t startAddress,
        uint16_t quantity);

    static ModbusEncodeResult encodeWriteSingleRegister(
        uint16_t transactionId,
        uint8_t unitId,
        uint16_t address,
        uint16_t value);

    static ModbusEncodeResult encodeWriteMultipleRegisters(
        uint16_t transactionId,
        uint8_t unitId,
        uint16_t startAddress,
        const std::vector<uint16_t>& values);
};

}
