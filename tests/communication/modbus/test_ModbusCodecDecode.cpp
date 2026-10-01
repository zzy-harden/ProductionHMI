#include "communication/modbus/ModbusCodec.h"

#include <QtTest/QtTest>

#include <initializer_list>
#include <vector>

using ProductionHMI::Communication::ModbusCodec;
using ProductionHMI::Communication::ModbusDecodeError;
using ProductionHMI::Communication::ModbusResponseExpectation;

namespace {

std::vector<uint8_t> bytes(std::initializer_list<uint8_t> values)
{
    return std::vector<uint8_t>(values);
}

} // namespace

class ModbusCodecDecodeTest : public QObject {
    Q_OBJECT

private slots:
    void decodesReadHoldingRegisters();
    void decodesReadInputRegisters();
    void decodesWriteSingleRegister();
    void decodesWriteMultipleRegisters();
    void reportsExceptionResponse();
    void rejectsFrameTooShort();
    void rejectsInvalidProtocolId();
    void rejectsLengthMismatch();
    void rejectsTransactionMismatch();
    void rejectsUnitIdMismatch();
    void rejectsFunctionMismatch();
    void rejectsOddReadByteCount();
    void rejectsUnexpectedReadQuantity();
    void rejectsUnsupportedFunction();
};

void ModbusCodecDecodeTest::decodesReadHoldingRegisters()
{
    const ModbusResponseExpectation expectation{0x0001, 0x01, 0x03, 2};
    const auto result = ModbusCodec::decodeResponse(
        bytes({
            0x00, 0x01, 0x00, 0x00, 0x00, 0x07,
            0x01, 0x03, 0x04, 0x12, 0x34, 0x56, 0x78
        }),
        expectation);

    QVERIFY2(result.success, result.message.c_str());
    QCOMPARE(result.response.transactionId, static_cast<uint16_t>(0x0001));
    QCOMPARE(result.response.unitId, static_cast<uint8_t>(0x01));
    QCOMPARE(result.response.functionCode, static_cast<uint8_t>(0x03));
    QCOMPARE(result.response.registers, std::vector<uint16_t>({0x1234, 0x5678}));
}

void ModbusCodecDecodeTest::decodesReadInputRegisters()
{
    const ModbusResponseExpectation expectation{0x002A, 0x11, 0x04, 1};
    const auto result = ModbusCodec::decodeResponse(
        bytes({
            0x00, 0x2A, 0x00, 0x00, 0x00, 0x05,
            0x11, 0x04, 0x02, 0x00, 0x64
        }),
        expectation);

    QVERIFY2(result.success, result.message.c_str());
    QCOMPARE(result.response.registers, std::vector<uint16_t>({0x0064}));
}

void ModbusCodecDecodeTest::decodesWriteSingleRegister()
{
    const ModbusResponseExpectation expectation{0x0003, 0x01, 0x06, std::nullopt};
    const auto result = ModbusCodec::decodeResponse(
        bytes({
            0x00, 0x03, 0x00, 0x00, 0x00, 0x06,
            0x01, 0x06, 0x00, 0x10, 0x12, 0x34
        }),
        expectation);

    QVERIFY2(result.success, result.message.c_str());
    QCOMPARE(result.response.writeAddress, static_cast<uint16_t>(0x0010));
    QCOMPARE(result.response.writeValue, static_cast<uint16_t>(0x1234));
}

void ModbusCodecDecodeTest::decodesWriteMultipleRegisters()
{
    const ModbusResponseExpectation expectation{0x0004, 0x01, 0x10, std::nullopt};
    const auto result = ModbusCodec::decodeResponse(
        bytes({
            0x00, 0x04, 0x00, 0x00, 0x00, 0x06,
            0x01, 0x10, 0x00, 0x20, 0x00, 0x02
        }),
        expectation);

    QVERIFY2(result.success, result.message.c_str());
    QCOMPARE(result.response.writeAddress, static_cast<uint16_t>(0x0020));
    QCOMPARE(result.response.writeQuantity, static_cast<uint16_t>(0x0002));
}

void ModbusCodecDecodeTest::reportsExceptionResponse()
{
    const ModbusResponseExpectation expectation{0x0005, 0x01, 0x03, 1};
    const auto result = ModbusCodec::decodeResponse(
        bytes({
            0x00, 0x05, 0x00, 0x00, 0x00, 0x03,
            0x01, 0x83, 0x02
        }),
        expectation);

    QVERIFY(!result.success);
    QCOMPARE(result.error, ModbusDecodeError::ExceptionResponse);
    QCOMPARE(result.response.exceptionCode, static_cast<uint8_t>(0x02));
}

void ModbusCodecDecodeTest::rejectsFrameTooShort()
{
    const ModbusResponseExpectation expectation{1, 1, 0x03, 1};
    const auto result = ModbusCodec::decodeResponse(bytes({0x00, 0x01, 0x00}), expectation);

    QVERIFY(!result.success);
    QCOMPARE(result.error, ModbusDecodeError::FrameTooShort);
}

void ModbusCodecDecodeTest::rejectsInvalidProtocolId()
{
    const ModbusResponseExpectation expectation{1, 1, 0x03, 1};
    const auto result = ModbusCodec::decodeResponse(
        bytes({
            0x00, 0x01, 0x00, 0x01, 0x00, 0x05,
            0x01, 0x03, 0x02, 0x00, 0x01
        }),
        expectation);

    QVERIFY(!result.success);
    QCOMPARE(result.error, ModbusDecodeError::InvalidProtocolId);
}

void ModbusCodecDecodeTest::rejectsLengthMismatch()
{
    const ModbusResponseExpectation expectation{1, 1, 0x03, 1};
    const auto result = ModbusCodec::decodeResponse(
        bytes({
            0x00, 0x01, 0x00, 0x00, 0x00, 0x07,
            0x01, 0x03, 0x02, 0x00, 0x01
        }),
        expectation);

    QVERIFY(!result.success);
    QCOMPARE(result.error, ModbusDecodeError::InvalidLength);
}

void ModbusCodecDecodeTest::rejectsTransactionMismatch()
{
    const ModbusResponseExpectation expectation{1, 1, 0x03, 1};
    const auto result = ModbusCodec::decodeResponse(
        bytes({
            0x00, 0x02, 0x00, 0x00, 0x00, 0x05,
            0x01, 0x03, 0x02, 0x00, 0x01
        }),
        expectation);

    QVERIFY(!result.success);
    QCOMPARE(result.error, ModbusDecodeError::TransactionMismatch);
}

void ModbusCodecDecodeTest::rejectsUnitIdMismatch()
{
    const ModbusResponseExpectation expectation{1, 1, 0x03, 1};
    const auto result = ModbusCodec::decodeResponse(
        bytes({
            0x00, 0x01, 0x00, 0x00, 0x00, 0x05,
            0x02, 0x03, 0x02, 0x00, 0x01
        }),
        expectation);

    QVERIFY(!result.success);
    QCOMPARE(result.error, ModbusDecodeError::UnitIdMismatch);
}

void ModbusCodecDecodeTest::rejectsFunctionMismatch()
{
    const ModbusResponseExpectation expectation{1, 1, 0x03, 1};
    const auto result = ModbusCodec::decodeResponse(
        bytes({
            0x00, 0x01, 0x00, 0x00, 0x00, 0x05,
            0x01, 0x04, 0x02, 0x00, 0x01
        }),
        expectation);

    QVERIFY(!result.success);
    QCOMPARE(result.error, ModbusDecodeError::FunctionMismatch);
}

void ModbusCodecDecodeTest::rejectsOddReadByteCount()
{
    const ModbusResponseExpectation expectation{1, 1, 0x03, std::nullopt};
    const auto result = ModbusCodec::decodeResponse(
        bytes({
            0x00, 0x01, 0x00, 0x00, 0x00, 0x06,
            0x01, 0x03, 0x03, 0x00, 0x01, 0x02
        }),
        expectation);

    QVERIFY(!result.success);
    QCOMPARE(result.error, ModbusDecodeError::InvalidPayload);
}

void ModbusCodecDecodeTest::rejectsUnexpectedReadQuantity()
{
    const ModbusResponseExpectation expectation{1, 1, 0x03, 2};
    const auto result = ModbusCodec::decodeResponse(
        bytes({
            0x00, 0x01, 0x00, 0x00, 0x00, 0x05,
            0x01, 0x03, 0x02, 0x00, 0x01
        }),
        expectation);

    QVERIFY(!result.success);
    QCOMPARE(result.error, ModbusDecodeError::InvalidPayload);
}

void ModbusCodecDecodeTest::rejectsUnsupportedFunction()
{
    const ModbusResponseExpectation expectation{1, 1, 0x01, std::nullopt};
    const auto result = ModbusCodec::decodeResponse(
        bytes({
            0x00, 0x01, 0x00, 0x00, 0x00, 0x03,
            0x01, 0x01, 0x00
        }),
        expectation);

    QVERIFY(!result.success);
    QCOMPARE(result.error, ModbusDecodeError::UnsupportedFunction);
}

QTEST_APPLESS_MAIN(ModbusCodecDecodeTest)
#include "test_ModbusCodecDecode.moc"
