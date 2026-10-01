# ProductionHMI 快速学习路线

这部分文档不是完整教程，而是配合仓库代码快速跟学的“任务笔记”。目标是让你知道每一步为什么做、改了哪些文件、哪些知识点必须掌握，以及面试时应该能讲清楚什么。

## 推荐学习方式

每个任务按这个顺序看：

```text
1. 先看对应学习记录
2. 再看头文件 / 接口
3. 再看 .cpp 实现
4. 最后看测试
5. 到 GitHub PR 的 Files changed 看增量
6. 不看文档，自己回答文末面试题
```

不要一上来逐行啃所有源码。

---

## 当前学习记录

| 任务 | 主题 | 你必须掌握的核心 |
|---|---|---|
| [T01](./T01-工程骨架.md) | C++ / Qt 工程骨架 | CMake target、Qt 事件循环、为什么 UI 线程不能阻塞 |
| [T02](./T02-核心领域模型.md) | 核心领域模型 | `enum class`、`variant`、`optional`、TagDefinition/TagValue、数据质量 |
| [T03](./T03-JSON配置加载.md) | 配置驱动 | JSON → 强类型对象、格式校验与业务校验、错误返回 |
| [T04](./T04-Modbus请求编码.md) | Modbus TCP 请求编码 | 寄存器、功能码、MBAP、Transaction ID、大端序、逐字节测试 |
| [T05](./T05-Modbus响应解析.md) | Modbus TCP 响应解析 | MBAP Length、请求响应匹配、异常响应、错误分类、Payload 校验 |

---

## 代码学习地图

```text
T01
CMakeLists.txt
src/app/main.cpp
   ↓

T02
src/core/domain/
   ↓

T03
config/project.example.json
src/core/config/
tests/core/config/
   ↓

T04 / T05
src/communication/modbus/ModbusCodec.*
tests/communication/modbus/test_ModbusCodecEncode.cpp
tests/communication/modbus/test_ModbusCodecDecode.cpp
```

你可以把目前项目理解成五步逐渐长出来：

```text
能构建和启动
   ↓
有统一业务数据模型
   ↓
能从配置文件生成业务对象
   ↓
能把 Modbus 请求编码成协议字节
   ↓
能验证并解析 PLC 返回的 Modbus 响应
```

下一阶段会进入 PLC 模拟器和真实 TCP 会话，把目前的“纯协议逻辑”接到真正的网络收发上。

---

## GitHub 怎么配合学习

- PR #1：集中包含了早期 T01~T03 的工程整理；因此这三个任务无法在 Git 历史中完全做到一任务一个 PR。
- PR #2：T04，可以直接通过 `Files changed` 查看本任务的增量。
- PR #3：补齐 T01~T04 学习记录。
- 从 T05 开始：坚持“一任务一 PR + 一份中文学习记录 + 自动测试 + 中文任务复盘”。

重点看 PR 的：

```text
Conversation   看任务目标和验收
Files changed  看这一任务到底改了什么
Checks         看 Windows 构建和测试是否通过
```

---

## 学习标准

不用要求自己能默写所有实现。更重要的是做到三点：

1. **能说清为什么有这个模块**；
2. **能画出它与上下游模块的关系**；
3. **能解释 2~3 个关键设计和测试点**。

做到这三点，项目才会逐渐变成你自己能讲的项目，而不是一份只能运行的代码。