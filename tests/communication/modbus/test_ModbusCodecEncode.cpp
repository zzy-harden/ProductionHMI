#include "communication/modbus/ModbusCodec.h"

#include <QtTest/QtTest>

#include <initializer_list>
#include <vector>

using ProductionHMI::Communication::ModbusCodec;
using ProductionHMI::Communication::ModbusEncodeError;
using ProductionHMI::Communication::ModbusEncodeResult;

namespace {

std::vector<uint8_t> bytes(std::initializer_list<uint8_t> values)
{
    return std::vector<uint8_t>(values);
}

void compareBytes(const ModbusEncodeResult& result, const std::vector<uint8_t>& expected)
{
    QVERIFY2(result.success, result.message.c_str());
    QCOMPARE(result.bytes, expected);
}

} // namespace

class ModbusCodecEncodeTest : public QObject {
    Q_OBJECT

private slots:
    void encodesReadHoldingRegisters();
    void encodesReadInputRegisters();
    void encodesWriteSingleRegister();
    void encodesWriteMultipleRegisters();
    void allowsLastRegisterForSingleRead();
    void rejectsZeroReadQuantity();
    void rejectsTooManyReadRegisters();
    void rejectsReadAddressOverflow();
    void rejectsEmptyMultipleWrite();
    void rejectsTooManyMultipleWriteRegisters();
    void rejectsMultipleWriteAddressOverflow();
};

void ModbusCodecEncodeTest::encodesReadHoldingRegisters()
{
    const auto result = ModbusCodec::encodeReadHoldingRegisters(0x0001, 0x01, 0x006B, 0x0003);
    compareBytes(result, bytes({
        0x00, 0x01, 0x00, 0x00, 0x00, 0x06,
        0x01, 0x03, 0x00, 0x6B, 0x00, 0x03
    }));
}

void ModbusCodecEncodeTest::encodesReadInputRegisters()
{
    const auto result = ModbusCodec::encodeReadInputRegisters(0x1234, 0x11, 0x0008, 0x0002);
    compareBytes(result, bytes({
        0x12, 0x34, 0x00, 0x00, 0x00, 0x06,
        0x11, 0x04, 0x00, 0x08, 0x00, 0x02
    }));
}

void ModbusCodecEncodeTest::encodesWriteSingleRegister()
{
    const auto result = ModbusCodec::encodeWriteSingleRegister(0x0003, 0x01, 0x0010, 0x1234);
    compareBytes(result, bytes({
        0x00, 0x03, 0x00, 0x00, 0x00, 0x06,
        0x01, 0x06, 0x00, 0x10, 0x12, 0x34
    }));
}

void ModbusCodecEncodeTest::encodesWriteMultipleRegisters()
{
    const auto result = ModbusCodec::encodeWriteMultipleRegisters(
        0x0004,
        0x01,
        0x0001,
        {0x000A, 0x0102});

    compareBytes(result, bytes({
        0x00, 0x04, 0x00, 0x00, 0x00, 0x0B,
        0x01, 0x10, 0x00, 0x01, 0x00, 0x02,
        0x04, 0x00, 0x0A, 0x01, 0x02
    }));
}

void ModbusCodecEncodeTest::allowsLastRegisterForSingleRead()
{
    const auto result = ModbusCodec::encodeReadHoldingRegisters(1, 1, 65535, 1);
    QVERIFY(result.success);
}

void ModbusCodecEncodeTest::rejectsZeroReadQuantity()
{
    const auto result = ModbusCodec::encodeReadHoldingRegisters(1, 1, 0, 0);
    QVERIFY(!result.success);
    QCOMPARE(result.error, ModbusEncodeError::InvalidQuantity);
}

void ModbusCodecEncodeTest::rejectsTooManyReadRegisters()
{
    const auto result = ModbusCodec::encodeReadInputRegisters(1, 1, 0, 126);
    QVERIFY(!result.success);
    QCOMPARE(result.error, ModbusEncodeError::InvalidQuantity);
}

void ModbusCodecEncodeTest::rejectsReadAddressOverflow()
{
    const auto result = ModbusCodec::encodeReadHoldingRegisters(1, 1, 65535, 2);
    QVERIFY(!result.success);
    QCOMPARE(result.error, ModbusEncodeError::AddressRangeOverflow);
}

void ModbusCodecEncodeTest::rejectsEmptyMultipleWrite()
{
    const auto result = ModbusCodec::encodeWriteMultipleRegisters(1, 1, 0, {});
    QVERIFY(!result.success);
    QCOMPARE(result.error, ModbusEncodeError::InvalidQuantity);
}

void ModbusCodecEncodeTest::rejectsTooManyMultipleWriteRegisters()
{
    const auto result = ModbusCodec::encodeWriteMultipleRegisters(1, 1, 0, std::vector<uint16_t>(124, 0));
    QVERIFY(!result.success);
    QCOMPARE(result.error, ModbusEncodeError::InvalidQuantity);
}

void ModbusCodecEncodeTest::rejectsMultipleWriteAddressOverflow()
{
    const auto result = ModbusCodec::encodeWriteMultipleRegisters(1, 1, 65535, {1, 2});
    QVERIFY(!result.success);
    QCOMPARE(result.error, ModbusEncodeError::AddressRangeOverflow);
}

QTEST_APPLESS_MAIN(ModbusCodecEncodeTest)
#include "test_ModbusCodecEncode.moc"
