# T03：JSON 配置加载——让项目从“写死代码”变成“配置驱动”

> 这一任务最重要的不是 JSON 语法，而是理解工业软件为什么强调“换设备尽量改配置，不改代码”。

## 1. 这一任务解决什么问题

如果把测点直接写在 C++ 里：

```cpp
// 反例
readRegister(100); // 温度
readRegister(101); // 压力
```

一旦 PLC 地址、测点数量、扫描周期改变，就必须改代码、重新编译、重新发布。

T03 的目标是把这些信息放到：

```text
config/project.example.json
```

程序启动时读取 JSON，并生成 T02 定义好的：

```text
DeviceConfig
TagDefinition
AlarmConfig
WriteRange
```

核心链路是：

```text
JSON 文件
   ↓
QJsonDocument / QJsonObject
   ↓
ProjectConfigLoader
   ↓
校验
   ↓
DeviceConfig
```

---

## 2. 必会：为什么选择 JSON

对于本项目，JSON 的优点：

- 人能直接阅读；
- 层级结构适合描述 device / tags / alarm；
- Qt 自带 JSON 解析能力；
- 不需要为了配置再引入额外数据库。

典型结构：

```json
{
  "device": {
    "id": "plc_001",
    "ip": "127.0.0.1",
    "port": 502
  },
  "tags": [
    {
      "id": "temperature",
      "address": 100,
      "dataType": "float32",
      "scanPeriodMs": 500
    }
  ]
}
```

这里需要区分两个概念：

```text
JSON
只是外部文本格式

DeviceConfig
才是程序内部真正使用的 C++ 类型
```

业务代码不应该到处直接操作 JSON。

---

## 3. 必会：为什么要有 `ProjectConfigLoader`

接口：

```cpp
class ProjectConfigLoader {
public:
    ConfigLoadResult loadFromFile(const std::string& path) const;
};
```

它把“读取文件 + JSON 解析 + 字段校验 + 类型转换”集中在一个地方。

后续采集代码只拿到：

```cpp
DeviceConfig device;
```

它不需要知道配置到底来自 JSON、数据库还是其他来源。

这就是模块边界。

---

## 4. 必会：Qt JSON 基础类

当前实现使用 Qt Core：

```cpp
QFile
QJsonDocument
QJsonObject
QJsonArray
QJsonValue
```

理解到下面程度就够：

```text
QFile
  ↓ 读取文件字节
QJsonDocument::fromJson(...)
  ↓ 解析整份 JSON
QJsonObject
  ↓ 访问键值对象
QJsonArray
  ↓ 遍历 tags 数组
```

例如：

```cpp
const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
```

这一行完成的是“文本字节 → JSON 文档”。

但**解析成功不代表配置合法**。

例如下面 JSON 语法完全正确：

```json
{
  "port": -100
}
```

业务上仍然非法。

因此必须继续做字段校验。

---

## 5. 必会：语法校验和业务校验不是一回事

T03 做了两层检查。

### 第一层：JSON 语法

例如：

```text
少逗号
括号不匹配
非法 JSON
```

由 `QJsonDocument` 负责发现。

### 第二层：业务合法性

例如：

```text
port 必须 1~65535
Unit ID 必须 0~255
scanPeriodMs 必须是支持的周期
Tag ID 不能重复
32 位数据不能从最后一个寄存器开始
writeRange 只能用于可写测点
```

这些必须由我们自己的代码检查。

### 面试高频表达

> 我把“格式解析”和“业务校验”分开。JSON 能成功 parse 只能证明格式合法，不能证明字段值对工业业务有效，所以加载阶段还会检查地址范围、类型、重复 ID、扫描周期和写入范围。

---

## 6. `ConfigLoadResult` 为什么不只返回一个对象

当前定义：

```cpp
struct ConfigLoadResult {
    bool success{false};
    DeviceConfig device;
    std::vector<std::string> errors;
};
```

如果函数只返回：

```cpp
DeviceConfig
```

那么失败时就不知道原因。

现在可以得到：

```text
success = false
errors = [
  "device.port: must be between 1 and 65535",
  "tags[2].id: duplicate tag id"
]
```

这样未来 UI 可以给用户明确提示。

这属于很实用的工程设计：

> 错误不仅要被发现，还要能定位。

---

## 7. 必会：为什么用小解析函数

实现里有类似：

```text
requiredString(...)
requiredInt(...)
optionalNumber(...)
optionalBool(...)
parseDataType(...)
parseByteOrder(...)
```

目的不是炫技，而是避免主函数里反复写：

```cpp
if (!obj.contains(...)) ...
if (!obj.value(...).isString()) ...
```

这种重复逻辑。

例如 `parseDataType()` 负责：

```text
"uint16" → DataType::UInt16
"float32" → DataType::Float32
非法字符串 → 记录错误
```

配置层完成“字符串 → 强类型枚举”的转换后，后续业务层就不需要继续处理字符串。

---

## 8. 一个容易忽略的点：32 位值占两个寄存器

Modbus 单个寄存器是 16 bit。

而：

```text
UInt32
Int32
Float32
```

需要：

```text
2 × 16 bit = 32 bit
```

所以如果 32 位测点从地址 `65535` 开始：

```text
65535   第一个寄存器
65536   第二个寄存器 ← 已经越界
```

因此加载配置时就拒绝这种配置。

这个检查很值得在面试里讲，因为它说明你不是把 JSON “读出来就算完”。

---

## 9. 报警回差配置为什么要检查

例如高温报警：

```text
high = 80℃
highRecover = 75℃
```

逻辑是：

```text
> 80℃ 触发
< 75℃ 才恢复
```

所以一般要求：

```text
highRecover <= high
```

否则配置本身的语义就可能矛盾。

低限报警同理。

T03 只是负责检查配置，真正的报警状态机会在后续任务实现。

---

## 10. 本任务主要文件

| 文件 | 类型 | 作用 |
|---|---|---|
| `src/core/config/ProjectConfigLoader.h` | 新增 | 对外加载接口与结果类型 |
| `src/core/config/ProjectConfigLoader.cpp` | 新增 | JSON 解析、转换、校验 |
| `config/project.example.json` | 新增 | 一份完整示例工程配置 |
| `tests/core/config/test_ProjectConfigLoader.cpp` | 新增 | 正常与非法配置测试 |
| `CMakeLists.txt` | 修改 | 编译配置模块并加入测试 |

推荐阅读：

```text
project.example.json
      ↓
ProjectConfigLoader.h
      ↓
ProjectConfigLoader.cpp
      ↓
test_ProjectConfigLoader.cpp
```

先看“输入是什么、输出是什么”，最后再看实现细节。

---

## 11. 自动测试要学什么

这一任务测试至少关注：

```text
正常配置能成功解析
不存在文件会失败
JSON 语法错误会失败
缺少必填字段会失败
重复 Tag ID 会失败
非法 dataType 会失败
32 位地址越界会失败
```

测试的价值不是为了追求数量，而是固定行为：

> 以后重构配置解析时，只要这些测试继续通过，就能降低把旧功能改坏的概率。

---

## 12. 面试前至少能回答

1. 为什么工业上位机要做“测点配置化”？
2. 为什么 JSON parse 成功仍然需要业务校验？
3. `ProjectConfigLoader` 为什么应该独立成模块？
4. 为什么配置字符串进入业务层前要转换成 `enum class`？
5. 为什么 32 位 Modbus 数据需要占两个寄存器？
6. `ConfigLoadResult` 相比“失败直接返回空对象”有什么好处？
7. 当前配置层和后续采集层之间通过什么对象衔接？

你能回答最后一个问题时，应该能说出：

```text
JSON → ProjectConfigLoader → DeviceConfig / TagDefinition → 后续采集模块
```