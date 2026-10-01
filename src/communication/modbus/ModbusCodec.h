#pragma once

#include <cstdint>
#include <optional>
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

enum class ModbusDecodeError {
    None,
    FrameTooShort,
    InvalidProtocolId,
    InvalidLength,
    TransactionMismatch,
    UnitIdMismatch,
    FunctionMismatch,
    ExceptionResponse,
    InvalidPayload,
    UnsupportedFunction
};

struct ModbusResponseExpectation {
    uint16_t transactionId{0};
    uint8_t unitId{0};
    uint8_t functionCode{0};
    std::optional<uint16_t> readQuantity;
};

struct ModbusResponse {
    uint16_t transactionId{0};
    uint8_t unitId{0};
    uint8_t functionCode{0};
    std::vector<uint16_t> registers;
    uint16_t writeAddress{0};
    uint16_t writeValue{0};
    uint16_t writeQuantity{0};
    uint8_t exceptionCode{0};
};

struct ModbusDecodeResult {
    bool success{false};
    ModbusResponse response;
    ModbusDecodeError error{ModbusDecodeError::None};
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

    static ModbusDecodeResult decodeResponse(
        const std::vector<uint8_t>& bytes,
        const ModbusResponseExpectation& expectation);
};

}
