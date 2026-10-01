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
constexpr std::size_t kMbapPrefixSize = 6;
constexpr std::size_t kMinimumResponseSize = 8;

void appendUint16(std::vector<uint8_t>& bytes, uint16_t value)
{
    bytes.push_back(static_cast<uint8_t>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<uint8_t>(value & 0xFFU));
}

uint16_t readUint16(const std::vector<uint8_t>& bytes, std::size_t offset)
{
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(bytes[offset]) << 8U)
        | static_cast<uint16_t>(bytes[offset + 1U]));
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

ModbusEncodeResult encodeFailure(ModbusEncodeError error, std::string message)
{
    ModbusEncodeResult result;
    result.error = error;
    result.message = std::move(message);
    return result;
}

ModbusDecodeResult decodeFailure(
    ModbusDecodeError error,
    std::string message,
    ModbusResponse response = {})
{
    ModbusDecodeResult result;
    result.error = error;
    result.message = std::move(message);
    result.response = std::move(response);
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
        return encodeFailure(
            ModbusEncodeError::InvalidQuantity,
            "read quantity must be between 1 and 125 registers");
    }
    if (!addressRangeFits(startAddress, quantity)) {
        return encodeFailure(
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

bool isSupportedFunction(uint8_t functionCode)
{
    return functionCode == kReadHoldingRegisters
        || functionCode == kReadInputRegisters
        || functionCode == kWriteSingleRegister
        || functionCode == kWriteMultipleRegisters;
}

ModbusDecodeResult decodeReadResponse(
    const std::vector<uint8_t>& bytes,
    const ModbusResponseExpectation& expectation,
    ModbusResponse response)
{
    if (bytes.size() < 9U) {
        return decodeFailure(
            ModbusDecodeError::InvalidPayload,
            "read response is missing byte count",
            std::move(response));
    }

    const uint8_t byteCount = bytes[8];
    if (byteCount == 0U || (byteCount % 2U) != 0U) {
        return decodeFailure(
            ModbusDecodeError::InvalidPayload,
            "read response byte count must be a non-zero even value",
            std::move(response));
    }

    if (bytes.size() != 9U + static_cast<std::size_t>(byteCount)) {
        return decodeFailure(
            ModbusDecodeError::InvalidPayload,
            "read response byte count does not match payload size",
            std::move(response));
    }

    const uint16_t registerCount = static_cast<uint16_t>(byteCount / 2U);
    if (registerCount > kMaxReadRegisters) {
        return decodeFailure(
            ModbusDecodeError::InvalidPayload,
            "read response exceeds 125 registers",
            std::move(response));
    }
    if (expectation.readQuantity && registerCount != *expectation.readQuantity) {
        return decodeFailure(
            ModbusDecodeError::InvalidPayload,
            "read response register count does not match request",
            std::move(response));
    }

    response.registers.reserve(registerCount);
    for (std::size_t offset = 9U; offset < bytes.size(); offset += 2U) {
        response.registers.push_back(readUint16(bytes, offset));
    }

    ModbusDecodeResult result;
    result.success = true;
    result.response = std::move(response);
    return result;
}

ModbusDecodeResult decodeWriteResponse(
    const std::vector<uint8_t>& bytes,
    ModbusResponse response)
{
    if (bytes.size() != 12U) {
        return decodeFailure(
            ModbusDecodeError::InvalidPayload,
            "write response must contain exactly 12 bytes",
            std::move(response));
    }

    response.writeAddress = readUint16(bytes, 8U);
    if (response.functionCode == kWriteSingleRegister) {
        response.writeValue = readUint16(bytes, 10U);
    } else {
        response.writeQuantity = readUint16(bytes, 10U);
        if (response.writeQuantity == 0U || response.writeQuantity > kMaxWriteMultipleRegisters) {
            return decodeFailure(
                ModbusDecodeError::InvalidPayload,
                "write-multiple response quantity must be between 1 and 123",
                std::move(response));
        }
    }

    ModbusDecodeResult result;
    result.success = true;
    result.response = std::move(response);
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
        return encodeFailure(
            ModbusEncodeError::InvalidQuantity,
            "write quantity must be between 1 and 123 registers");
    }

    const auto quantity = static_cast<uint16_t>(values.size());
    if (!addressRangeFits(startAddress, quantity)) {
        return encodeFailure(
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

ModbusDecodeResult ModbusCodec::decodeResponse(
    const std::vector<uint8_t>& bytes,
    const ModbusResponseExpectation& expectation)
{
    if (bytes.size() < kMinimumResponseSize) {
        return decodeFailure(
            ModbusDecodeError::FrameTooShort,
            "response is shorter than MBAP header plus function code");
    }

    ModbusResponse response;
    response.transactionId = readUint16(bytes, 0U);
    const uint16_t protocolId = readUint16(bytes, 2U);
    const uint16_t length = readUint16(bytes, 4U);
    response.unitId = bytes[6];
    response.functionCode = bytes[7];

    if (protocolId != kProtocolId) {
        return decodeFailure(
            ModbusDecodeError::InvalidProtocolId,
            "protocol id must be 0 for Modbus TCP",
            std::move(response));
    }
    if (length < 2U || bytes.size() != kMbapPrefixSize + static_cast<std::size_t>(length)) {
        return decodeFailure(
            ModbusDecodeError::InvalidLength,
            "MBAP length does not match response size",
            std::move(response));
    }
    if (response.transactionId != expectation.transactionId) {
        return decodeFailure(
            ModbusDecodeError::TransactionMismatch,
            "transaction id does not match request",
            std::move(response));
    }
    if (response.unitId != expectation.unitId) {
        return decodeFailure(
            ModbusDecodeError::UnitIdMismatch,
            "unit id does not match request",
            std::move(response));
    }

    const bool isException = (response.functionCode & 0x80U) != 0U;
    const uint8_t baseFunction = static_cast<uint8_t>(response.functionCode & 0x7FU);
    if (isException) {
        if (baseFunction != expectation.functionCode) {
            return decodeFailure(
                ModbusDecodeError::FunctionMismatch,
                "exception function code does not match request",
                std::move(response));
        }
        if (bytes.size() != 9U || length != 3U) {
            return decodeFailure(
                ModbusDecodeError::InvalidPayload,
                "exception response must contain exactly one exception code",
                std::move(response));
        }
        response.exceptionCode = bytes[8];
        return decodeFailure(
            ModbusDecodeError::ExceptionResponse,
            "device returned a Modbus exception response",
            std::move(response));
    }

    if (response.functionCode != expectation.functionCode) {
        return decodeFailure(
            ModbusDecodeError::FunctionMismatch,
            "function code does not match request",
            std::move(response));
    }
    if (!isSupportedFunction(response.functionCode)) {
        return decodeFailure(
            ModbusDecodeError::UnsupportedFunction,
            "response function code is not supported",
            std::move(response));
    }

    if (response.functionCode == kReadHoldingRegisters
        || response.functionCode == kReadInputRegisters) {
        return decodeReadResponse(bytes, expectation, std::move(response));
    }

    return decodeWriteResponse(bytes, std::move(response));
}

} // namespace ProductionHMI::Communication
