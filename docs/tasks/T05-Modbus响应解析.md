# T05：Modbus TCP 响应解析——不要“收到 bytes 就当成功”

> T04 解决“怎么把请求编码成字节”；T05 做相反方向：把 PLC 返回的字节解析成结构化结果，并判断它到底是不是我们等待的那个合法响应。

## 1. 这一任务解决什么问题

以后 TCP 收到的数据只是：

```text
00 01 00 00 00 07 01 03 04 12 34 56 78
```

业务层不能直接拿这种字节数组工作。

所以要经过：

```text
原始 bytes
   ↓
ModbusCodec::decodeResponse()
   ↓
检查 MBAP / 请求匹配 / 异常响应 / Payload
   ↓
ModbusResponse
```

本任务只做**完整 Modbus TCP 响应帧的协议解析**。

暂时不做：

- TCP 半包/粘包缓存；
- 超时计时器；
- socket 生命周期；
- 自动重连。

这些属于后续通信会话任务。

---

## 2. 必会：为什么“能解析”不等于“这是正确响应”

假设你发送：

```text
Transaction ID = 10
Unit ID = 1
Function = 03
```

结果收到一帧合法 Modbus 报文，但：

```text
Transaction ID = 9
```

这可能是别的请求对应的响应。

因此解析时不只检查格式，还要检查：

```text
Protocol ID
Length
Transaction ID
Unit ID
Function Code
Payload
```

项目通过：

```cpp
ModbusResponseExpectation
```

保存“我原本期待什么响应”。

当前包含：

```cpp
transactionId
unitId
functionCode
readQuantity（读请求时可选）
```

---

## 3. 必会：MBAP 的 Length 到底表示什么

Modbus TCP 前 6 个字节是：

```text
Transaction ID  2B
Protocol ID     2B
Length          2B
```

`Length` 统计的是它后面的：

```text
Unit ID + PDU
```

因此完整 ADU 字节数可以计算成：

```text
6 + Length
```

项目会验证：

```cpp
bytes.size() == 6 + length
```

如果不一致，返回：

```text
InvalidLength
```

这个检查很重要，因为以后 TCP 是字节流，长度字段还会成为处理半包/粘包的依据。

---

## 4. 必会：正常响应和异常响应不是一种格式

如果请求功能码是：

```text
03
```

PLC 正常响应仍然是：

```text
03
```

但设备拒绝请求时，异常响应功能码会变成：

```text
03 | 0x80 = 0x83
```

然后带一个 Exception Code。

例如：

```text
00 05 00 00 00 03 01 83 02
```

其中：

```text
83 = 03 的异常响应
02 = Exception Code
```

项目不会把这种情况当作“解析失败”，而会明确分类为：

```text
ModbusDecodeError::ExceptionResponse
```

同时保存：

```cpp
response.exceptionCode
```

### 面试表达

> Modbus 异常响应本身可能是一帧格式完全合法的报文，所以不能简单以“收到响应”作为成功标准。需要识别功能码最高位，并把设备异常码向上层报告。

---

## 5. 03/04 读响应怎么解析

读寄存器响应结构可以先记成：

```text
Function Code
Byte Count
Register Data...
```

例如返回两个寄存器：

```text
03
04
12 34
56 78
```

这里：

```text
Byte Count = 4 byte
Register Count = 4 / 2 = 2
```

解析后：

```cpp
response.registers = {
    0x1234,
    0x5678
};
```

项目还检查：

- Byte Count 不能为 0；
- Byte Count 必须是偶数，因为一个寄存器 2 byte；
- Byte Count 必须和实际 Payload 长度一致；
- 若已知请求读取数量，则返回寄存器数必须和请求一致。

这样可以避免“报文截断但仍被错误接受”。

---

## 6. 06/10 写响应怎么解析

### 0x06 写单寄存器

PLC 正常响应会回显：

```text
写入地址
写入值
```

所以解析到：

```cpp
response.writeAddress
response.writeValue
```

### 0x10 写多个寄存器

正常响应回显：

```text
起始地址
成功写入的寄存器数量
```

解析到：

```cpp
response.writeAddress
response.writeQuantity
```

注意：

> “PLC 返回正常写响应”以后仍然不等于设备物理状态一定已经达到期望值。

真正的“写后读回确认”会在后续控制任务实现。

---

## 7. 必会：为什么错误要分类

当前解码错误包括：

```text
FrameTooShort
InvalidProtocolId
InvalidLength
TransactionMismatch
UnitIdMismatch
FunctionMismatch
ExceptionResponse
InvalidPayload
UnsupportedFunction
```

不要只返回：

```text
false
```

因为这些错误的处理策略以后不同：

```text
TransactionMismatch
→ 可能是请求响应关联问题

ExceptionResponse
→ PLC 明确拒绝请求

InvalidPayload
→ 协议数据异常

FrameTooShort
→ 以后可能说明 TCP 数据尚未收完整
```

### 面试表达

> 我把传输结果分层分类，避免所有问题都变成“通信失败”。协议错误、设备异常和请求响应不匹配后续可以采用不同处理策略。

---

## 8. 必会：为什么有 `readUint16()`

T04 有：

```cpp
appendUint16()
```

用于：

```text
uint16_t → 2 bytes
```

T05 新增相反方向：

```cpp
readUint16()
```

用于：

```text
2 bytes → uint16_t
```

例如：

```text
12 34 → 0x1234
```

编码和解码都显式处理协议字节序，不依赖主机内存布局。

---

## 9. 当前解码顺序要记住

`decodeResponse()` 大致按照下面顺序验证：

```text
最小长度
   ↓
读取 MBAP 基本字段
   ↓
Protocol ID
   ↓
Length
   ↓
Transaction ID
   ↓
Unit ID
   ↓
异常响应？
   ↓
Function Code
   ↓
03/04 或 06/10 的 Payload
```

这个顺序有一个基本原则：

> 越基础的格式条件越早检查，只有前面的结构可信，才继续访问更深层的字段。

---

## 10. 本任务主要文件

| 文件 | 类型 | 作用 |
|---|---|---|
| `src/communication/modbus/ModbusCodec.h` | 修改 | 增加解码结果、错误类型、响应期望 |
| `src/communication/modbus/ModbusCodec.cpp` | 修改 | 实现 MBAP / 正常响应 / 异常响应解析 |
| `tests/communication/modbus/test_ModbusCodecDecode.cpp` | 新增 | 正常响应和错误分类测试 |
| `CMakeLists.txt` | 修改 | 新增解码测试 target |
| `docs/tasks/T05-Modbus响应解析.md` | 新增 | 本学习记录 |

建议阅读：

```text
ModbusCodec.h
   ↓
看 ModbusResponseExpectation / DecodeResult
   ↓
ModbusCodec.cpp 的 decodeResponse()
   ↓
再看 decodeReadResponse / decodeWriteResponse
   ↓
最后看 test_ModbusCodecDecode.cpp
```

---

## 11. 测试重点看什么

这次测试覆盖两类。

### 正常路径

```text
03 响应
04 响应
06 响应
10 响应
```

### 异常路径

```text
帧太短
Protocol ID 错误
MBAP Length 错误
Transaction ID 不匹配
Unit ID 不匹配
Function Code 不匹配
异常响应 0x83
Byte Count 非法
返回寄存器数量不符合请求
不支持的功能码
```

协议解析代码往往“正常路径很短，异常路径很多”。这是正常现象。

---

## 12. 面试前至少能回答

1. Modbus TCP 的 `Length` 字段怎么算完整帧长度？
2. 为什么收到合法 Modbus 帧仍然可能不是当前请求的响应？
3. Transaction ID 的作用是什么？
4. Modbus 异常响应为什么会出现 `0x83` 这样的功能码？
5. 03/04 响应里的 Byte Count 为什么必须是偶数？
6. 为什么需要区分 `ExceptionResponse` 和 `InvalidPayload`？
7. 06 与 10 的正常响应分别回显什么？
8. 为什么 Codec 仍然不处理 TCP 半包/粘包？

最后一个问题建议回答：

> Codec 负责一帧完整 Modbus ADU 的协议语义；TCP 是字节流，如何从接收缓冲区切出完整帧属于会话/传输层职责，后续结合 MBAP Length 处理。