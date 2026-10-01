# T04：Modbus TCP 请求编码——第一次真正进入工业通信协议

> 如果你之前不了解 Modbus，这一节只先掌握项目当前真的用到的内容：寄存器、功能码、MBAP Header、请求报文、大端序、请求边界。不要一开始背完整 Modbus 标准。

## 1. 先知道 Modbus 在这个项目里干什么

我们的上位机和 PLC 之间通过 Modbus TCP 通信。

你可以先把它理解成：

```text
上位机（Client）
   │
   │ 请求：读取某些寄存器 / 写某个寄存器
   ▼
PLC（Server）
   │
   │ 响应：返回数据 / 返回写入结果
   ▼
上位机
```

T04 只做“请求编码”：

```text
函数参数
   ↓
ModbusCodec
   ↓
std::vector<uint8_t>
   ↓
以后交给 TCP socket 发送
```

本任务**不负责**：

- 建立 TCP 连接；
- 发送 socket；
- 解析响应；
- 超时；
- 重连。

保持一个任务只解决一个核心问题。

---

## 2. 必会：什么是“寄存器”

Modbus 世界里，可以先把寄存器理解成 PLC 暴露出来的一格一格数据位置。

本项目当前关心：

```text
Holding Register   保持寄存器
Input Register     输入寄存器
```

一个 Modbus Register 是：

```text
16 bit = 2 byte
```

因此：

```text
UInt16    1 个寄存器
Float32   2 个寄存器
UInt32    2 个寄存器
```

T02/T03 中为什么要区分 `DataType`，现在就开始有意义了。

---

## 3. 必会：当前四个功能码

T04 只实现四个功能码。

| 功能码 | 含义 | 当前接口 |
|---|---|---|
| `0x03` | 读保持寄存器 | `encodeReadHoldingRegisters` |
| `0x04` | 读输入寄存器 | `encodeReadInputRegisters` |
| `0x06` | 写单个保持寄存器 | `encodeWriteSingleRegister` |
| `0x10` | 写多个保持寄存器 | `encodeWriteMultipleRegisters` |

面试时至少要能说清楚：

> 03/04 是读，06/10 是写；03 读 Holding Register，04 读 Input Register；06 写一个 16 位寄存器，10 可以连续写多个寄存器。

---

## 4. 必会：Modbus TCP 报文由什么组成

请求大体可以拆成：

```text
MBAP Header + PDU
```

### MBAP Header

```text
Transaction ID   2 byte
Protocol ID      2 byte
Length           2 byte
Unit ID          1 byte
```

### PDU

不同功能码内容不同，例如 03 请求：

```text
Function Code    1 byte
Start Address    2 byte
Quantity         2 byte
```

因此一个典型 03 请求是：

```text
┌──────────── MBAP ────────────┐ ┌──── PDU ────┐
Transaction Protocol Length Unit Function Addr Count
```

---

## 5. Transaction ID 是什么

它是 Modbus TCP 很重要的字段。

例如上位机发：

```text
Transaction ID = 100
```

PLC 的对应响应也应该带：

```text
Transaction ID = 100
```

后续 T05/T08 会用它做请求响应匹配。

你现在先记：

> Transaction ID 不是寄存器地址，也不是设备地址，它用于关联 TCP 上的一次请求和对应响应。

---

## 6. Protocol ID 和 Unit ID

### Protocol ID

Modbus TCP 中这里通常是：

```text
0x0000
```

代码里：

```cpp
constexpr uint16_t kProtocolId = 0;
```

### Unit ID

当前接口传入：

```cpp
uint8_t unitId
```

它可用于标识目标从站/网关后的设备。项目配置中也提前保留了这个字段。

---

## 7. 必会：为什么要自己处理大端序

例如 16 位整数：

```text
0x1234
```

Modbus 报文按高字节在前编码：

```text
12 34
```

项目统一封装：

```cpp
void appendUint16(std::vector<uint8_t>& bytes, uint16_t value)
{
    bytes.push_back((value >> 8) & 0xFF);
    bytes.push_back(value & 0xFF);
}
```

核心思想：

```text
16 bit value
  ↓ 拆分
高 8 bit
低 8 bit
  ↓
按协议顺序写入 byte vector
```

### 面试高频点

**Q：为什么不能直接把 `uint16_t` 内存拷到网络报文？**

答：

> 主机字节序可能不同，而协议规定了网络中的字节顺序。直接拷贝会依赖机器端序，所以显式按协议顺序编码更可靠。

---

## 8. 看懂一个完整 03 请求

假设：

```text
Transaction ID = 1
Unit ID        = 1
功能码         = 03
起始地址       = 0x006B
数量           = 3
```

最终字节：

```text
00 01    Transaction ID
00 00    Protocol ID
00 06    Length
01       Unit ID
03       Function Code
00 6B    Start Address
00 03    Quantity
```

你学习 T04 时，最值得做的事情不是背函数，而是亲手把这个报文按字段拆一遍。

---

## 9. 必会：为什么 Codec 不持有 QTcpSocket

当前设计：

```text
ModbusCodec
只负责：协议字节

QTcpSocket（后续）
只负责：TCP 收发
```

而不是：

```text
一个巨大类
既 connect
又发 socket
又拼 Modbus
又解析
又重连
```

好处：

- 协议编码可以不联网就测试；
- 网络层和协议层能独立修改；
- 出错更容易定位；
- 测试只需要比较字节数组。

### 面试表达

> 我把协议编解码做成无状态 Codec，和 socket 生命周期解耦。这样协议层是纯逻辑，容易做逐字节单元测试，网络层只负责连接和收发。

---

## 10. 当前 `ModbusCodec` 的返回值

接口不是只返回：

```cpp
std::vector<uint8_t>
```

而是：

```cpp
struct ModbusEncodeResult {
    bool success;
    std::vector<uint8_t> bytes;
    ModbusEncodeError error;
    std::string message;
};
```

这样非法请求可以在发送前被拒绝，并说明原因。

当前编码错误包括：

```text
InvalidQuantity
AddressRangeOverflow
```

---

## 11. 必会：为什么读最多 125 个，写多个最多 123 个

当前代码限制：

```cpp
kMaxReadRegisters = 125;
kMaxWriteMultipleRegisters = 123;
```

这是 Modbus PDU 大小限制推导出的常见标准上限。

初学阶段不用死背完整计算过程，但面试中至少知道：

> 这不是随便拍的业务数字，而是协议报文长度限制决定的，因此编码层必须检查。

---

## 12. 地址范围为什么也要检查

Modbus 地址字段只有 16 bit：

```text
0 ~ 65535
```

例如：

```text
start = 65535
quantity = 1
```

合法。

但：

```text
start = 65535
quantity = 2
```

第二个地址会变成 65536，超出 16 位范围。

项目中先提升为 `uint32_t` 做计算：

```cpp
const uint32_t lastAddress =
    static_cast<uint32_t>(startAddress)
    + static_cast<uint32_t>(quantity) - 1U;
```

这是一个值得学的 C++ 小细节：

> 做边界计算时先扩宽类型，避免窄整数自身发生溢出后再判断。

---

## 13. 0x10 为什么比 0x06 复杂

`0x06` 只写一个寄存器：

```text
Function
Address
Value
```

`0x10` 需要：

```text
Function
Start Address
Quantity
Byte Count
Value 1
Value 2
...
```

因此代码需要根据：

```cpp
values.size()
```

计算：

```text
quantity
byteCount = quantity * 2
MBAP length
```

这也是为什么多寄存器写更容易出现长度字段错误，必须用测试固定下来。

---

## 14. 本任务主要文件

| 文件 | 类型 | 作用 |
|---|---|---|
| `src/communication/modbus/ModbusCodec.h` | 新增 | 对外请求编码接口、错误类型 |
| `src/communication/modbus/ModbusCodec.cpp` | 新增 | MBAP 与 03/04/06/10 编码实现 |
| `tests/communication/modbus/test_ModbusCodecEncode.cpp` | 新增 | 精确字节与非法参数测试 |
| `CMakeLists.txt` | 修改 | 把 Codec 和测试加入工程 |
| `README.md` / `docs/开发状态.md` | 修改 | 更新任务状态 |

本任务对应 GitHub：

```text
PR #2
```

学习时建议直接打开 PR #2 的 **Files changed** 看增量。

---

## 15. 测试为什么一定要比较“完整字节”

协议代码最怕这种情况：

```text
长度字段错 1
功能码对了但地址高低字节反了
Transaction ID 写错位置
```

程序可能依然成功编译，但 PLC 不会按你的预期处理。

所以测试不是只检查：

```cpp
result.success == true
```

而是比较：

```text
实际生成的每一个 byte
vs
预期协议报文的每一个 byte
```

这叫协议一致性测试。

---

## 16. 你应该亲手做一次

拿纸或记事本，自己手算：

```text
Transaction ID = 0x002A
Unit ID = 1
功能码 = 03
起始地址 = 100
数量 = 5
```

然后对照测试代码，看你算出的完整字节序列是否一致。

如果这一步能独立完成，你才算真正理解“请求编码”。

---

## 17. 面试前至少能回答

1. Modbus TCP 中一个寄存器是多少 bit？
2. 03 / 04 / 06 / 10 分别是什么？
3. MBAP Header 有哪些主要字段？
4. Transaction ID 用来做什么？
5. 为什么编码多字节整数时要关注端序？
6. 为什么 `ModbusCodec` 不直接负责 TCP 连接？
7. 为什么读寄存器数量和写多个寄存器数量要设上限？
8. 为什么检查 `startAddress + quantity - 1` 时先转成更宽的整数？
9. 单元测试为什么应该比较完整协议字节？

下一步 T05 会在此基础上做相反方向：

```text
PLC 返回的原始 bytes
        ↓
ModbusCodec
        ↓
结构化响应 / 明确错误
```

所以 T04 不理解清楚，T05 会更难。