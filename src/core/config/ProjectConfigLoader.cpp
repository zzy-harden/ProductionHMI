#include "ProjectConfigLoader.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <set>
#include <string>

namespace ProductionHMI::Core {
namespace {

void addError(ConfigLoadResult& result, const std::string& location, const std::string& message)
{
    result.errors.push_back(location + ": " + message);
}

std::optional<QString> requiredString(
    const QJsonObject& object,
    const char* key,
    const std::string& location,
    ConfigLoadResult& result)
{
    const auto value = object.value(QLatin1String(key));
    if (!value.isString() || value.toString().trimmed().isEmpty()) {
        addError(result, location + "." + key, "must be a non-empty string");
        return std::nullopt;
    }
    return value.toString().trimmed();
}

std::optional<int> requiredInt(
    const QJsonObject& object,
    const char* key,
    const std::string& location,
    ConfigLoadResult& result)
{
    const auto value = object.value(QLatin1String(key));
    if (!value.isDouble()) {
        addError(result, location + "." + key, "must be an integer");
        return std::nullopt;
    }

    const double number = value.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number
        || number < static_cast<double>(std::numeric_limits<int>::min())
        || number > static_cast<double>(std::numeric_limits<int>::max())) {
        addError(result, location + "." + key, "must be an integer");
        return std::nullopt;
    }

    return static_cast<int>(number);
}

std::optional<double> optionalNumber(
    const QJsonObject& object,
    const char* key,
    const std::string& location,
    ConfigLoadResult& result,
    double defaultValue)
{
    if (!object.contains(QLatin1String(key))) {
        return defaultValue;
    }

    const auto value = object.value(QLatin1String(key));
    if (!value.isDouble() || !std::isfinite(value.toDouble())) {
        addError(result, location + "." + key, "must be a finite number");
        return std::nullopt;
    }
    return value.toDouble();
}

std::optional<bool> optionalBool(
    const QJsonObject& object,
    const char* key,
    const std::string& location,
    ConfigLoadResult& result,
    bool defaultValue)
{
    if (!object.contains(QLatin1String(key))) {
        return defaultValue;
    }

    const auto value = object.value(QLatin1String(key));
    if (!value.isBool()) {
        addError(result, location + "." + key, "must be a boolean");
        return std::nullopt;
    }
    return value.toBool();
}

std::optional<RegisterType> parseRegisterType(
    const QString& text,
    const std::string& location,
    ConfigLoadResult& result)
{
    if (text.compare("holding", Qt::CaseInsensitive) == 0) {
        return RegisterType::HoldingRegister;
    }
    if (text.compare("input", Qt::CaseInsensitive) == 0) {
        return RegisterType::InputRegister;
    }
    addError(result, location, "unsupported registerType; expected holding or input");
    return std::nullopt;
}

std::optional<DataType> parseDataType(
    const QString& text,
    const std::string& location,
    ConfigLoadResult& result)
{
    if (text.compare("uint16", Qt::CaseInsensitive) == 0) return DataType::UInt16;
    if (text.compare("int16", Qt::CaseInsensitive) == 0) return DataType::Int16;
    if (text.compare("uint32", Qt::CaseInsensitive) == 0) return DataType::UInt32;
    if (text.compare("int32", Qt::CaseInsensitive) == 0) return DataType::Int32;
    if (text.compare("float32", Qt::CaseInsensitive) == 0) return DataType::Float32;
    if (text.compare("bit", Qt::CaseInsensitive) == 0) return DataType::Bit;

    addError(result, location, "unsupported dataType");
    return std::nullopt;
}

std::optional<ByteOrder> parseByteOrder(
    const QString& text,
    const std::string& location,
    ConfigLoadResult& result)
{
    if (text.compare("ABCD", Qt::CaseInsensitive) == 0) return ByteOrder::ABCD;
    if (text.compare("BADC", Qt::CaseInsensitive) == 0) return ByteOrder::BADC;
    if (text.compare("CDAB", Qt::CaseInsensitive) == 0) return ByteOrder::CDAB;
    if (text.compare("DCBA", Qt::CaseInsensitive) == 0) return ByteOrder::DCBA;

    addError(result, location, "unsupported byteOrder");
    return std::nullopt;
}

bool usesTwoRegisters(DataType type)
{
    return type == DataType::UInt32 || type == DataType::Int32 || type == DataType::Float32;
}

bool validScanPeriod(int value)
{
    constexpr std::array<int, 5> supported{100, 200, 500, 1000, 5000};
    for (const int period : supported) {
        if (period == value) return true;
    }
    return false;
}

std::optional<AlarmConfig> parseAlarm(
    const QJsonObject& tagObject,
    const std::string& location,
    ConfigLoadResult& result)
{
    if (!tagObject.contains("alarm")) {
        return std::nullopt;
    }
    if (!tagObject.value("alarm").isObject()) {
        addError(result, location + ".alarm", "must be an object");
        return std::nullopt;
    }

    const QJsonObject alarmObject = tagObject.value("alarm").toObject();
    AlarmConfig alarm;

    const auto enabled = optionalBool(alarmObject, "enabled", location + ".alarm", result, true);
    const auto high = optionalNumber(alarmObject, "high", location + ".alarm", result, 0.0);
    const auto highRecover = optionalNumber(alarmObject, "highRecover", location + ".alarm", result, high.value_or(0.0));
    const auto low = optionalNumber(alarmObject, "low", location + ".alarm", result, 0.0);
    const auto lowRecover = optionalNumber(alarmObject, "lowRecover", location + ".alarm", result, low.value_or(0.0));

    if (!enabled || !high || !highRecover || !low || !lowRecover) {
        return std::nullopt;
    }

    int delayMs = 0;
    if (alarmObject.contains("delayMs")) {
        const auto parsedDelay = requiredInt(alarmObject, "delayMs", location + ".alarm", result);
        if (!parsedDelay) return std::nullopt;
        delayMs = *parsedDelay;
    }
    if (delayMs < 0) {
        addError(result, location + ".alarm.delayMs", "must be >= 0");
        return std::nullopt;
    }

    if (*highRecover > *high) {
        addError(result, location + ".alarm.highRecover", "must be <= high");
    }
    if (*lowRecover < *low) {
        addError(result, location + ".alarm.lowRecover", "must be >= low");
    }
    if (*low > *high) {
        addError(result, location + ".alarm", "low must be <= high");
    }

    alarm.enabled = *enabled;
    alarm.highLimit = *high;
    alarm.highRecover = *highRecover;
    alarm.lowLimit = *low;
    alarm.lowRecover = *lowRecover;
    alarm.delayMs = static_cast<uint32_t>(delayMs);
    return alarm;
}

std::optional<WriteRange> parseWriteRange(
    const QJsonObject& tagObject,
    bool writable,
    const std::string& location,
    ConfigLoadResult& result)
{
    if (!tagObject.contains("writeRange")) {
        return std::nullopt;
    }
    if (!tagObject.value("writeRange").isObject()) {
        addError(result, location + ".writeRange", "must be an object");
        return std::nullopt;
    }

    if (!writable) {
        addError(result, location + ".writeRange", "is only valid for writable tags");
        return std::nullopt;
    }

    const QJsonObject rangeObject = tagObject.value("writeRange").toObject();
    if (!rangeObject.value("min").isDouble() || !rangeObject.value("max").isDouble()) {
        addError(result, location + ".writeRange", "min and max must be numbers");
        return std::nullopt;
    }

    const double minimum = rangeObject.value("min").toDouble();
    const double maximum = rangeObject.value("max").toDouble();
    if (!std::isfinite(minimum) || !std::isfinite(maximum) || minimum > maximum) {
        addError(result, location + ".writeRange", "requires finite min <= max");
        return std::nullopt;
    }

    return WriteRange{minimum, maximum};
}

} // namespace

ConfigLoadResult ProjectConfigLoader::loadFromFile(const std::string& path) const
{
    ConfigLoadResult result;

    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::ReadOnly)) {
        addError(result, "file", "cannot open config file: " + path);
        return result;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        addError(result, "json", parseError.errorString().toStdString());
        return result;
    }
    if (!document.isObject()) {
        addError(result, "json", "root must be an object");
        return result;
    }

    const QJsonObject root = document.object();
    if (!root.value("device").isObject()) {
        addError(result, "device", "must be an object");
        return result;
    }
    if (!root.value("tags").isArray()) {
        addError(result, "tags", "must be an array");
        return result;
    }

    const QJsonObject deviceObject = root.value("device").toObject();
    const auto deviceId = requiredString(deviceObject, "id", "device", result);
    const auto deviceName = requiredString(deviceObject, "name", "device", result);
    const auto ip = requiredString(deviceObject, "ip", "device", result);
    const auto port = requiredInt(deviceObject, "port", "device", result);
    const auto unitId = requiredInt(deviceObject, "unitId", "device", result);
    const auto timeoutMs = requiredInt(deviceObject, "timeoutMs", "device", result);
    const auto retryCount = requiredInt(deviceObject, "retryCount", "device", result);

    if (port && (*port < 1 || *port > 65535)) {
        addError(result, "device.port", "must be between 1 and 65535");
    }
    if (unitId && (*unitId < 0 || *unitId > 255)) {
        addError(result, "device.unitId", "must be between 0 and 255");
    }
    if (timeoutMs && *timeoutMs <= 0) {
        addError(result, "device.timeoutMs", "must be > 0");
    }
    if (retryCount && (*retryCount < 0 || *retryCount > 10)) {
        addError(result, "device.retryCount", "must be between 0 and 10");
    }

    const QJsonArray tags = root.value("tags").toArray();
    if (tags.isEmpty()) {
        addError(result, "tags", "must contain at least one tag");
    }

    std::set<std::string> seenTagIds;
    std::vector<TagDefinition> parsedTags;
    parsedTags.reserve(static_cast<std::size_t>(tags.size()));

    for (qsizetype i = 0; i < tags.size(); ++i) {
        const std::string location = "tags[" + std::to_string(i) + "]";
        if (!tags.at(i).isObject()) {
            addError(result, location, "must be an object");
            continue;
        }

        const QJsonObject tagObject = tags.at(i).toObject();
        const auto id = requiredString(tagObject, "id", location, result);
        const auto name = requiredString(tagObject, "name", location, result);
        const auto registerTypeText = requiredString(tagObject, "registerType", location, result);
        const auto address = requiredInt(tagObject, "address", location, result);
        const auto dataTypeText = requiredString(tagObject, "dataType", location, result);
        const auto byteOrderText = requiredString(tagObject, "byteOrder", location, result);
        const auto unit = requiredString(tagObject, "unit", location, result);
        const auto scanPeriod = requiredInt(tagObject, "scanPeriodMs", location, result);
        const auto scale = optionalNumber(tagObject, "scale", location, result, 1.0);
        const auto offset = optionalNumber(tagObject, "offset", location, result, 0.0);
        const auto writable = optionalBool(tagObject, "writable", location, result, false);

        if (!id || !name || !registerTypeText || !address || !dataTypeText || !byteOrderText
            || !unit || !scanPeriod || !scale || !offset || !writable) {
            continue;
        }

        const auto registerType = parseRegisterType(*registerTypeText, location + ".registerType", result);
        const auto dataType = parseDataType(*dataTypeText, location + ".dataType", result);
        const auto byteOrder = parseByteOrder(*byteOrderText, location + ".byteOrder", result);
        if (!registerType || !dataType || !byteOrder) {
            continue;
        }

        const std::string tagId = id->toStdString();
        if (!seenTagIds.insert(tagId).second) {
            addError(result, location + ".id", "duplicate tag id: " + tagId);
        }
        if (*address < 0 || *address > 65535) {
            addError(result, location + ".address", "must be between 0 and 65535");
        } else if (usesTwoRegisters(*dataType) && *address == 65535) {
            addError(result, location + ".address", "32-bit value cannot start at register 65535");
        }
        if (!validScanPeriod(*scanPeriod)) {
            addError(result, location + ".scanPeriodMs", "supported values are 100, 200, 500, 1000, 5000");
        }
        if (*scale == 0.0) {
            addError(result, location + ".scale", "must not be 0");
        }

        const auto alarm = parseAlarm(tagObject, location, result);
        const auto writeRange = parseWriteRange(tagObject, *writable, location, result);

        TagDefinition tag;
        tag.id = tagId;
        tag.name = name->toStdString();
        tag.registerType = *registerType;
        tag.address = static_cast<uint16_t>(std::clamp(*address, 0, 65535));
        tag.dataType = *dataType;
        tag.byteOrder = *byteOrder;
        tag.scale = *scale;
        tag.offset = *offset;
        tag.unit = unit->toStdString();
        tag.writable = *writable;
        tag.scanPeriodMs = static_cast<uint32_t>(std::max(*scanPeriod, 0));
        tag.writeRange = writeRange;
        tag.alarm = alarm;
        parsedTags.push_back(std::move(tag));
    }

    if (!result.errors.empty()) {
        return result;
    }

    result.device.id = deviceId->toStdString();
    result.device.name = deviceName->toStdString();
    result.device.ip = ip->toStdString();
    result.device.port = static_cast<uint16_t>(*port);
    result.device.unitId = static_cast<uint8_t>(*unitId);
    result.device.timeoutMs = static_cast<uint32_t>(*timeoutMs);
    result.device.retryCount = static_cast<uint32_t>(*retryCount);
    result.device.tags = std::move(parsedTags);
    result.success = true;
    return result;
}

} // namespace ProductionHMI::Core
