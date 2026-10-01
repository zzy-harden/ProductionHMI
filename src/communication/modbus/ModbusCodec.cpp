#include "ModbusCodec.h"

#include <limits>
#include <utility>

namespace ProductionHMI::Communication {
namespace {

constexpr uint16_t kProtocolId = 0;
constexpr uint8_t kReadHoldingRegisters = 0x03;
constexpr uint8_t kReadInputRegisters = 0x04;
constexpr uint8_t kWriteSingleRegister = 0x06;
constexpr uint8_t kWriteMultipleRegisters = 0x10;
constexpr uint16_t kMaxReadRegisters = 125;
constexpr uint16_t kMaxWriteMultipleRegisters = 123;

void appendUint16(std::vector<uint8_t>& bytes, uint16_t value)
{
    bytes.push_back(static_cast<uint8_t>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<uint8_t>(value & 0xFFU));
}

bool addressRangeFits(uint16_t startAddress, uint16_t quantity)
{
    if (quantity == 0) {
        return false;
    }

    const uint32_t lastAddress = static_cast<uint32_t>(startAddress)
        + static_cast<uint32_t>(quantity) - 1U;
    return lastAddress <= std::numeric_limits<uint16_t>::max();
}

ModbusEncodeResult failure(ModbusEncodeError error, std::string message)
{
    ModbusEncodeResult result;
    result.error = error;
    result.message = std::move(message);
    return result;
}

void appendMbapHeader(
    std::vector<uint8_t>& bytes,
    uint16_t transactionId,
    uint16_t length,
    uint8_t unitId)
{
    appendUint16(bytes, transactionId);
    appendUint16(bytes, kProtocolId);
    appendUint16(bytes, length);
    bytes.push_back(unitId);
}

ModbusEncodeResult encodeReadRegisters(
    uint8_t functionCode,
    uint16_t transactionId,
    uint8_t unitId,
    uint16_t startAddress,
    uint16_t quantity)
{
    if (quantity == 0 || quantity > kMaxReadRegisters) {
        return failure(
            ModbusEncodeError::InvalidQuantity,
            "read quantity must be between 1 and 125 registers");
    }
    if (!addressRangeFits(startAddress, quantity)) {
        return failure(
            ModbusEncodeError::AddressRangeOverflow,
            "requested register range exceeds address 65535");
    }

    ModbusEncodeResult result;
    result.success = true;
    result.bytes.reserve(12);

    appendMbapHeader(result.bytes, transactionId, 6, unitId);
    result.bytes.push_back(functionCode);
    appendUint16(result.bytes, startAddress);
    appendUint16(result.bytes, quantity);
    return result;
}

} // namespace

ModbusEncodeResult ModbusCodec::encodeReadHoldingRegisters(
    uint16_t transactionId,
    uint8_t unitId,
    uint16_t startAddress,
    uint16_t quantity)
{
    return encodeReadRegisters(
        kReadHoldingRegisters,
        transactionId,
        unitId,
        startAddress,
        quantity);
}

ModbusEncodeResult ModbusCodec::encodeReadInputRegisters(
    uint16_t transactionId,
    uint8_t unitId,
    uint16_t startAddress,
    uint16_t quantity)
{
    return encodeReadRegisters(
        kReadInputRegisters,
        transactionId,
        unitId,
        startAddress,
        quantity);
}

ModbusEncodeResult ModbusCodec::encodeWriteSingleRegister(
    uint16_t transactionId,
    uint8_t unitId,
    uint16_t address,
    uint16_t value)
{
    ModbusEncodeResult result;
    result.success = true;
    result.bytes.reserve(12);

    appendMbapHeader(result.bytes, transactionId, 6, unitId);
    result.bytes.push_back(kWriteSingleRegister);
    appendUint16(result.bytes, address);
    appendUint16(result.bytes, value);
    return result;
}

ModbusEncodeResult ModbusCodec::encodeWriteMultipleRegisters(
    uint16_t transactionId,
    uint8_t unitId,
    uint16_t startAddress,
    const std::vector<uint16_t>& values)
{
    if (values.empty() || values.size() > kMaxWriteMultipleRegisters) {
        return failure(
            ModbusEncodeError::InvalidQuantity,
            "write quantity must be between 1 and 123 registers");
    }

    const auto quantity = static_cast<uint16_t>(values.size());
    if (!addressRangeFits(startAddress, quantity)) {
        return failure(
            ModbusEncodeError::AddressRangeOverflow,
            "requested register range exceeds address 65535");
    }

    const uint16_t byteCount = static_cast<uint16_t>(quantity * 2U);
    const uint16_t length = static_cast<uint16_t>(7U + byteCount);

    ModbusEncodeResult result;
    result.success = true;
    result.bytes.reserve(static_cast<std::size_t>(13U + byteCount));

    appendMbapHeader(result.bytes, transactionId, length, unitId);
    result.bytes.push_back(kWriteMultipleRegisters);
    appendUint16(result.bytes, startAddress);
    appendUint16(result.bytes, quantity);
    result.bytes.push_back(static_cast<uint8_t>(byteCount));

    for (const uint16_t value : values) {
        appendUint16(result.bytes, value);
    }

    return result;
}

} // namespace ProductionHMI::Communication
