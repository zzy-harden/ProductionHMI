#include "core/config/ProjectConfigLoader.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using ProductionHMI::Core::ByteOrder;
using ProductionHMI::Core::DataType;
using ProductionHMI::Core::ProjectConfigLoader;
using ProductionHMI::Core::RegisterType;

namespace {

QString writeConfig(QTemporaryDir& dir, const QByteArray& content)
{
    const QString path = dir.filePath("project.json");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return {};
    }
    file.write(content);
    file.close();
    return path;
}

const QByteArray kValidConfig = R"json(
{
  "device": {
    "id": "plc_001",
    "name": "Demo PLC",
    "ip": "127.0.0.1",
    "port": 5020,
    "unitId": 1,
    "timeoutMs": 1000,
    "retryCount": 2
  },
  "tags": [
    {
      "id": "temperature",
      "name": "Temperature",
      "registerType": "holding",
      "address": 100,
      "dataType": "Float32",
      "byteOrder": "CDAB",
      "scale": 0.1,
      "offset": -20.0,
      "unit": "C",
      "writable": true,
      "scanPeriodMs": 200,
      "writeRange": { "min": 0.0, "max": 100.0 },
      "alarm": {
        "enabled": true,
        "high": 80.0,
        "highRecover": 75.0,
        "low": 5.0,
        "lowRecover": 8.0,
        "delayMs": 3000
      }
    }
  ]
}
)json";

} // namespace

class ProjectConfigLoaderTest : public QObject {
    Q_OBJECT

private slots:
    void loadsValidConfig();
    void rejectsMissingFile();
    void rejectsInvalidJson();
    void rejectsMissingRequiredField();
    void rejectsDuplicateTagId();
    void rejectsInvalidDataType();
    void rejectsInvalid32BitStartAddress();
};

void ProjectConfigLoaderTest::loadsValidConfig()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeConfig(dir, kValidConfig);
    QVERIFY(!path.isEmpty());

    const auto result = ProjectConfigLoader{}.loadFromFile(path.toStdString());

    QVERIFY2(result.success, result.errors.empty() ? "unknown error" : result.errors.front().c_str());
    QCOMPARE(result.device.id, std::string("plc_001"));
    QCOMPARE(result.device.port, static_cast<uint16_t>(5020));
    QCOMPARE(result.device.tags.size(), std::size_t{1});

    const auto& tag = result.device.tags.front();
    QCOMPARE(tag.id, std::string("temperature"));
    QCOMPARE(tag.registerType, RegisterType::HoldingRegister);
    QCOMPARE(tag.dataType, DataType::Float32);
    QCOMPARE(tag.byteOrder, ByteOrder::CDAB);
    QCOMPARE(tag.address, static_cast<uint16_t>(100));
    QCOMPARE(tag.scanPeriodMs, uint32_t{200});
    QVERIFY(tag.writeRange.has_value());
    QCOMPARE(tag.writeRange->minimum, 0.0);
    QCOMPARE(tag.writeRange->maximum, 100.0);
    QVERIFY(tag.alarm.has_value());
    QCOMPARE(tag.alarm->delayMs, uint32_t{3000});
}

void ProjectConfigLoaderTest::rejectsMissingFile()
{
    const auto result = ProjectConfigLoader{}.loadFromFile("missing-project-config.json");
    QVERIFY(!result.success);
    QVERIFY(!result.errors.empty());
}

void ProjectConfigLoaderTest::rejectsInvalidJson()
{
    QTemporaryDir dir;
    const QString path = writeConfig(dir, "{ this is not json }");
    const auto result = ProjectConfigLoader{}.loadFromFile(path.toStdString());
    QVERIFY(!result.success);
    QVERIFY(!result.errors.empty());
}

void ProjectConfigLoaderTest::rejectsMissingRequiredField()
{
    QTemporaryDir dir;
    QByteArray config = kValidConfig;
    config.replace("\"ip\": \"127.0.0.1\",", "");
    const QString path = writeConfig(dir, config);

    const auto result = ProjectConfigLoader{}.loadFromFile(path.toStdString());
    QVERIFY(!result.success);
    QVERIFY(!result.errors.empty());
}

void ProjectConfigLoaderTest::rejectsDuplicateTagId()
{
    QTemporaryDir dir;
    QByteArray config = kValidConfig;
    const QByteArray tag = R"json(,
    {
      "id": "temperature",
      "name": "Duplicate",
      "registerType": "holding",
      "address": 200,
      "dataType": "UInt16",
      "byteOrder": "ABCD",
      "unit": "x",
      "scanPeriodMs": 1000
    })json";
    config.replace("\n  ]\n}", tag + "\n  ]\n}");
    const QString path = writeConfig(dir, config);

    const auto result = ProjectConfigLoader{}.loadFromFile(path.toStdString());
    QVERIFY(!result.success);
    QVERIFY(!result.errors.empty());
}

void ProjectConfigLoaderTest::rejectsInvalidDataType()
{
    QTemporaryDir dir;
    QByteArray config = kValidConfig;
    config.replace("\"Float32\"", "\"Float64\"");
    const QString path = writeConfig(dir, config);

    const auto result = ProjectConfigLoader{}.loadFromFile(path.toStdString());
    QVERIFY(!result.success);
    QVERIFY(!result.errors.empty());
}

void ProjectConfigLoaderTest::rejectsInvalid32BitStartAddress()
{
    QTemporaryDir dir;
    QByteArray config = kValidConfig;
    config.replace("\"address\": 100", "\"address\": 65535");
    const QString path = writeConfig(dir, config);

    const auto result = ProjectConfigLoader{}.loadFromFile(path.toStdString());
    QVERIFY(!result.success);
    QVERIFY(!result.errors.empty());
}

QTEST_MAIN(ProjectConfigLoaderTest)
#include "test_ProjectConfigLoader.moc"
