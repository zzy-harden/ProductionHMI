# ProductionHMI

面向自动化产线设备的 Windows HMI 上位机项目。

## 当前技术栈

- C++17
- Qt 6 Widgets
- CMake
- Modbus TCP（后续任务实现）
- SQLite（后续任务实现）
- QCustomPlot（后续任务实现）

## 当前进度

- T01：工程骨架 ✅
- T02：核心领域模型 ✅
- T03：JSON 工程配置加载 ✅

T03 已通过 Windows GitHub Actions 验证：Qt 6.8.3 + MSVC 2022 x64 下完成 CMake 配置、编译和自动测试。

项目严格按照 `docs/执行计划.md` 的任务顺序开发；当前任务验证失败时先修复，不携带已知失败进入后续任务。

## Windows 11 开发环境

推荐：

- Qt Creator
- Qt 6.8.x
- MSVC 2022 x64
- CMake 3.20+

Qt Creator 直接打开仓库根目录 `CMakeLists.txt` 即可。

命令行构建：

```powershell
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

## 配置文件

示例：`config/project.example.json`

工程配置负责描述 PLC 连接参数、测点地址、数据类型、扫描周期、报警规则及可写参数范围。核心业务配置不使用 QSettings。
