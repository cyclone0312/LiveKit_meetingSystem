# 命令行开发速查手册

> **MeetingApp 项目 - 常用命令行操作及原理解析**  
> 最后更新：2026年2月7日

---

用户运行 build.cmd
    ↓
[环境初始化]
vcvarsall.bat x64
    ↓ 设置 PATH, INCLUDE, LIB
[CMake 配置]
cmake -S . -B build/release -G Ninja //定义源码在哪（Source），编译后的中间产物放在哪（Build）。这样可以保持源码目录干净（Out-of-source build）
    ↓
FetchLiveKit.cmake (检查/下载 SDK)
    ↓
find_package(Qt6) (找到 Qt 库)
    ↓
生成 build.ninja
    ↓
[编译]
ninja -C build/release -j 8
    ↓
cl.exe 编译 .cpp → .obj
    ↓
link.exe 链接 .obj + .lib → .exe
    ↓
复制 DLL 到输出目录
    ↓
[完成]
build/release/MeetingApp.exe

## 📑 目录

1. [构建命令](#1-构建命令)
2. [CMake 核心命令](#2-cmake-核心命令)
3. [调试与运行](#3-调试与运行)
4. [依赖管理](#4-依赖管理)
5. [项目维护](#5-项目维护)
6. [问题排查](#6-问题排查)
7. [原理深入解析](#7-原理深入解析)

---

## 1. 构建命令

### 🚀 快速构建（推荐）

```powershell
# === 默认 Release 构建 ===
build.cmd
# 或显式指定
build.cmd release

# === Debug 构建（⚠️ 注意版本兼容性）===
build.cmd debug
# 警告：当前 Debug SDK 版本较旧，可能与新代码不兼容
# 推荐只使用 Release 构建

# === 清理构建产物 ===
build.cmd clean

# === 完全重新构建 ===
build.cmd rebuild
```

**原理**：

- `build.cmd` 封装了 `vcvarsall.bat` + `cmake` + `ninja` 的完整流程
- 自动初始化 MSVC 编译环境（设置 PATH、INCLUDE、LIB 等环境变量）
- 调用 CMake 生成 Ninja 构建文件，然后执行并行编译

---

## 2. CMake 核心命令

### 📐 配置项目（Configure）

```powershell
# === 基础配置 ===
cmake -S . -B build/release ^
    -G Ninja ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_PREFIX_PATH="D:/qt6.4/6.8.3/msvc2022_64"

# 参数说明：
# -S .                     源代码目录（当前目录）
# -B build/release         构建输出目录
# -G Ninja                 使用 Ninja 生成器（比 MSBuild 快）
# -DCMAKE_BUILD_TYPE       构建类型：Release/Debug/RelWithDebInfo
# -DCMAKE_PREFIX_PATH      Qt 安装路径（用于 find_package）
```

**原理**：

1. CMake 读取 `CMakeLists.txt`
2. 执行 `include(cmake/FetchLiveKit.cmake)` → 检查/下载 SDK
3. 执行 `find_package(Qt6 ...)` → 查找 Qt 组件
4. 生成 `build.ninja` 文件（包含所有编译规则）

### 🔨 构建项目（Build）

```powershell
# === 构建所有目标 ===
cmake --build build/release

# === 并行构建（8 线程）===
cmake --build build/release -j 8

# === 详细输出（查看编译命令）===
cmake --build build/release --verbose

# === 只构建特定目标 ===
cmake --build build/release --target MeetingApp

# === 清理构建产物 ===
cmake --build build/release --target clean
```

**原理**：

```
cmake --build
    ↓
调用 Ninja (CMAKE_MAKE_PROGRAM)
    ↓
Ninja 读取 build.ninja
    ↓
分析依赖关系（哪些文件修改了？）
    ↓
并行执行编译任务:
  - cl.exe /c src/main.cpp /Fobuild/main.cpp.obj
  - cl.exe /c src/livekitmanager.cpp /Fobuild/livekitmanager.cpp.obj
    ↓
链接成可执行文件:
  - link.exe /OUT:MeetingApp.exe *.obj livekit.lib Qt6Core.lib
    ↓
执行 POST_BUILD 命令（复制 DLL）
```

### 🔧 高级配置选项

```powershell
# === 生成编译命令数据库（用于 IDE 代码智能提示）===
cmake -S . -B build/release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
# 生成 build/release/compile_commands.json

# === 显示所有 CMake 变量 ===
cmake -L build/release

# === 显示详细变量（包括高级选项）===
cmake -LAH build/release

# === 修改缓存变量（不重新配置）===
cmake -B build/release -DLIVEKIT_VERSION=0.2.9

# === 清除 CMake 缓存并重新配置 ===
Remove-Item build/release/CMakeCache.txt
cmake -S . -B build/release
```

---

## 3. 调试与运行

### ▶️ 直接运行

```powershell
# === 运行 Release 版本 ===
.\build\release\MeetingApp.exe

# === 运行 Debug 版本 ===
.\build\debug\MeetingApp.exe

# === 带命令行参数运行 ===
.\build\release\MeetingApp.exe --help
.\build\release\MeetingApp.exe --room test123 --token eyJhbG...

# === 查看进程信息 ===
Get-Process MeetingApp
# 或查看详细信息
Get-Process MeetingApp | Format-List *
```

### 🐛 调试工具

#### **方法 1: Visual Studio (最强大)**

```powershell
# 1. 生成 VS 解决方案（只需一次）
cmake -S . -B build/vs ^
    -G "Visual Studio 17 2022" ^
    -A x64 ^
    -DCMAKE_PREFIX_PATH="D:/qt6.4/6.8.3/msvc2022_64"

# 2. 打开项目
start build/vs/MeetingApp.sln

# 3. 在 Visual Studio 中：
#    - 右键 MeetingApp 项目 → 设为启动项目
#    - 设置断点（F9）
#    - 按 F5 开始调试
#    - F10 逐过程，F11 逐语句，F5 继续
```

**调试快捷键**：

- `F5` - 启动调试/继续运行
- `F9` - 设置/取消断点
- `F10` - 逐过程（Step Over）
- `F11` - 逐语句（Step Into）
- `Shift+F11` - 跳出函数（Step Out）
- `Ctrl+F10` - 运行到光标处

#### **方法 2: VS Code**

```powershell
# 1. 构建 Debug 版本
build.cmd debug

# 2. 创建 .vscode/launch.json
```

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "调试 MeetingApp",
      "type": "cppvsdbg",
      "request": "launch",
      "program": "${workspaceFolder}/build/debug/MeetingApp.exe",
      "args": [],
      "stopAtEntry": false,
      "cwd": "${workspaceFolder}",
      "environment": [],
      "console": "externalTerminal"
    }
  ]
}
```

```powershell
# 3. 在 VS Code 中按 F5 启动调试
```

#### **方法 3: WinDbg (底层调试)**

```powershell
# 启动 WinDbg
"C:\Program Files\Windows Kits\10\Debuggers\x64\windbg.exe" ^
    .\build\debug\MeetingApp.exe

# 常用命令：
# g           - 继续运行
# bp main     - 在 main 函数设置断点
# k           - 查看调用堆栈
# dt ClassName - 显示类结构
# !analyze -v - 崩溃分析
```

### 📊 性能分析

```powershell
# === 使用 Visual Studio Profiler ===
# 1. 打开 VS
# 2. 调试 → 性能探查器
# 3. 选择工具（CPU 使用率、内存使用率、GPU 使用率等）
# 4. 点击"开始"

# === 查看内存使用 ===
Get-Process MeetingApp | Select-Object WorkingSet64, PrivateMemorySize64
# 输出：
# WorkingSet64        : 52428800  (50 MB 工作集)
# PrivateMemorySize64 : 41943040  (40 MB 私有内存)
```

---

## 4. 依赖管理

### 📦 LiveKit SDK 管理

```powershell
# === 查看当前 SDK 版本 ===
Get-Content extend\livekit-sdk-windows\.livekit-*.stamp
# 输出：LiveKit SDK v0.2.7 downloaded and extracted successfully.

# === 升级到新版本 ===
# 1. 编辑 cmake/FetchLiveKit.cmake
#    set(LIVEKIT_VERSION "0.2.9" ...)

# 2. 删除旧的 stamp 文件
Remove-Item extend\livekit-sdk-windows\.livekit-*.stamp

# 3. 重新配置（会自动下载新版）
build.cmd rebuild

# === 手动清理 SDK 缓存 ===
Remove-Item extend\.cache -Recurse -Force

# === 验证 SDK 文件完整性 ===
Test-Path extend\livekit-sdk-windows\bin\livekit.dll
Test-Path extend\livekit-sdk-windows\include\livekit\livekit.h
```

**SDK 目录结构**：

```
extend/livekit-sdk-windows/
├── bin/
│   ├── livekit.dll          ← 核心库
│   └── livekit_ffi.dll      ← FFI 接口库
├── include/livekit/
│   ├── livekit.h            ← 主头文件
│   ├── room.h
│   ├── participant.h
│   └── ...
├── lib/
│   ├── livekit.lib          ← 导入库
│   ├── livekit_ffi.dll.lib
│   └── cmake/LiveKit/       ← CMake 配置文件
└── .livekit-0.2.7.stamp     ← 版本标记
```

### 🔧 Qt 依赖检查

```powershell
# === 查看编译时链接的 Qt 库 ===
cmake --build build/release --verbose 2>&1 | Select-String "Qt6"

# === 检查运行时 Qt DLL ===
Get-ChildItem build\release\*.dll | Where-Object {$_.Name -like "Qt6*"}
# 输出：
# Qt6Core.dll
# Qt6Gui.dll
# Qt6Quick.dll
# Qt6QuickControls2.dll
# ...

# === 使用 windeployqt 收集所有 Qt 依赖 ===
D:\qt6.4\6.8.3\msvc2022_64\bin\windeployqt.exe ^
    --release ^
    --qmldir resources\qml ^
    --dry-run ^  # 只显示会复制哪些文件，不实际复制
    .\build\release\MeetingApp.exe
```

### 🔍 依赖项分析工具

```powershell
# === Dependencies (图形化 DLL 依赖查看器) ===
# 下载: https://github.com/lucasg/Dependencies
# 用法：拖拽 MeetingApp.exe 到 Dependencies.exe

# === dumpbin (MSVC 自带) ===
# 查看 EXE 依赖的 DLL
dumpbin /DEPENDENTS build\release\MeetingApp.exe

# 查看导出符号
dumpbin /EXPORTS extend\livekit-sdk-windows\bin\livekit.dll
```

---

## 5. 项目维护

### 🧹 清理命令

```powershell
# === 清理构建产物但保留配置 ===
cmake --build build/release --target clean

# === 删除整个构建目录 ===
Remove-Item build -Recurse -Force

# === 清理 CMake 缓存（保留源码） ===
Remove-Item build\release\CMakeCache.txt
Remove-Item build\release\CMakeFiles -Recurse -Force

# === 清理 SDK 下载缓存 ===
Remove-Item extend\.cache -Recurse -Force

# === Git 清理未追踪文件（谨慎！） ===
git clean -fdx  # -f 强制 -d 目录 -x 包括 .gitignore 的文件
```

### 📝 代码格式化

```powershell
# === 使用 clang-format ===
# 安装: choco install llvm

# 格式化单个文件
clang-format -i src\main.cpp

# 格式化所有 C++ 文件
Get-ChildItem src -Recurse -Include *.cpp,*.h | ForEach-Object {
    clang-format -i $_.FullName
}

# 检查格式（不修改文件）
clang-format --dry-run --Werror src\main.cpp
```

### 🔬 静态代码分析

```powershell
# === clang-tidy (需要 compile_commands.json) ===
cmake -S . -B build/release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

clang-tidy src\main.cpp -p build\release

# === MSVC 静态分析 ===
# 在 CMakeLists.txt 中添加：
# target_compile_options(${PROJECT_NAME} PRIVATE /analyze)
```

---

## 6. 问题排查

### ❌ 常见错误及解决方案

#### **错误 1: "系统找不到指定的文件" (rc.exe/mt.exe)**

```
错误信息：
RC Pass 1: command "rc /fo ..." failed
系统找不到指定的文件。
```

**原因**：未初始化 MSVC 环境  
**解决**：使用 `build.cmd` 而不是直接运行 `cmake`

```powershell
# ❌ 错误做法
cmake -B build

# ✅ 正确做法
build.cmd
```

#### **错误 2: "找不到 Qt6Core.dll"**

```
错误信息：
无法启动此程序，因为计算机中丢失 Qt6Core.dll
```

**原因**：运行时 DLL 路径问题  
**解决**：

```powershell
# 检查 DLL 是否在可执行文件目录
Test-Path build\release\Qt6Core.dll

# 如果缺失，手动运行 windeployqt
D:\qt6.4\6.8.3\msvc2022_64\bin\windeployqt.exe build\release\MeetingApp.exe
```

#### **错误 3: LiveKit SDK 下载失败**

```
错误信息：
下载 LiveKit SDK 失败: Failed to connect to github.com
```

**解决**：

```powershell
# 方法1: 配置代理
$env:http_proxy = "http://127.0.0.1:7890"
$env:https_proxy = "http://127.0.0.1:7890"

# 方法2: 手动下载
# 1. 浏览器下载: https://github.com/livekit/client-sdk-cpp/releases/download/v0.2.7/livekit-sdk-windows-x64-0.2.7.zip
# 2. 放到: extend/.cache/livekit-sdk-windows-x64-0.2.7.zip
# 3. 重新运行: build.cmd rebuild
```

#### **错误 4: 链接错误 "unresolved external symbol"**

```
错误信息：
error LNK2019: 无法解析的外部符号 "public: void __cdecl livekit::Room::connect"
```

**排查步骤**：

```powershell
# 1. 检查库文件是否存在
Test-Path extend\livekit-sdk-windows\lib\livekit.lib

# 2. 检查符号是否导出
dumpbin /EXPORTS extend\livekit-sdk-windows\bin\livekit.dll | Select-String "connect"

# 3. 清理后重新构建
build.cmd rebuild
```

### 🔍 诊断命令

```powershell
# === 查看编译器版本 ===
cl.exe  # 需在 Developer Command Prompt 或运行 vcvarsall.bat 后

# === 查看 CMake 版本 ===
cmake --version

# === 查看 Ninja 版本 ===
ninja --version

# === 查看 Qt 版本 ===
D:\qt6.4\6.8.3\msvc2022_64\bin\qmake.exe -v

# === 查看所有环境变量 ===
Get-ChildItem Env:

# === 查看 PATH 变量（分行显示）===
$env:PATH -split ';'

# === 检查 DLL 依赖 ===
dumpbin /DEPENDENTS build\release\MeetingApp.exe
```

---

## 7. 原理深入解析

### 🛠️ MSVC 工具链初始化流程

```batch
用户运行 build.cmd
    ↓
执行: call "vcvarsall.bat" x64
    ↓
vcvarsall.bat 设置环境变量:
    ├─ PATH += C:\...\MSVC\14.44\bin\Hostx64\x64  (cl.exe, link.exe)
    ├─ PATH += C:\...\Windows Kits\10\bin\...    (rc.exe, mt.exe)
    ├─ INCLUDE += C:\...\MSVC\14.44\include      (标准库头文件)
    ├─ INCLUDE += C:\...\Windows Kits\10\Include (Windows SDK)
    ├─ LIB += C:\...\MSVC\14.44\lib\x64          (标准库 .lib)
    └─ LIB += C:\...\Windows Kits\10\Lib         (Windows API .lib)
    ↓
现在可以调用 cl.exe, link.exe, rc.exe 等工具
```

**关键文件**：

- `vcvarsall.bat` 位置: `C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat`
- 参数 `x64` 表示编译 64 位程序

### 🏗️ CMake 构建流程

```
cmake -S . -B build/release
    ↓
【配置阶段 (Configure)】
    ├─ 读取 CMakeLists.txt
    ├─ 执行 include(cmake/FetchLiveKit.cmake)
    │   └─ 检查 .stamp 文件 → 下载/解压 SDK → 设置变量
    ├─ 执行 find_package(Qt6 ...)
    │   └─ 根据 CMAKE_PREFIX_PATH 查找 Qt6Config.cmake
    ├─ 处理 target_link_libraries(...)
    └─ 生成 build.ninja (编译规则)
    ↓
cmake --build build/release
    ↓
【生成阶段 (Generate)】
    └─ 调用 ninja -C build/release
    ↓
【构建阶段 (Build)】
Ninja 读取 build.ninja:
    ├─ 检查依赖关系（哪些文件修改了？）
    ├─ 并行编译 .cpp → .obj
    │   └─ cl.exe /c /O2 /EHsc /std:c++17 src/main.cpp /Fosrc/main.cpp.obj
    ├─ 链接 .obj → .exe
    │   └─ link.exe /OUT:MeetingApp.exe *.obj livekit.lib Qt6Core.lib
    └─ 执行 POST_BUILD 命令
        └─ copy DLL 文件到输出目录
```

### 🔗 链接过程详解

```
源文件 (.cpp)
    ↓ [编译] cl.exe
目标文件 (.obj)  ← 包含机器码，但符号未解析
    ↓ [链接] link.exe
可执行文件 (.exe)
    ├─ 静态链接: 将 .lib 中的代码复制到 .exe
    └─ 动态链接: 记录 DLL 导入表，运行时加载
```

**导入库 (.lib) vs DLL 的关系**：

```
livekit.lib (导入库)
    ├─ 包含函数签名和 DLL 名称
    └─ 链接时用于解析符号

livekit.dll (动态链接库)
    ├─ 包含实际实现代码
    └─ 运行时加载（必须在 PATH 或 .exe 同目录）
```

### 📊 增量编译原理

Ninja 通过**时间戳比较**实现增量编译：

```
修改 src/main.cpp
    ↓
Ninja 检测到:
    main.cpp 修改时间 (2026-02-07 10:30) > main.cpp.obj 修改时间 (2026-02-07 10:00)
    ↓
标记 main.cpp.obj 需要重新生成
    ↓
检查依赖链:
    main.cpp.obj 变化 → MeetingApp.exe 需要重新链接
    ↓
只重新编译 main.cpp，其他文件跳过
    ↓
重新链接生成 MeetingApp.exe
```

---

## 🎓 总结

### 核心命令速查表

| 操作             | 命令                                           | 说明                      |
| ---------------- | ---------------------------------------------- | ------------------------- |
| **构建 Release** | `build.cmd`                                    | 推荐的构建方式            |
| **构建 Debug**   | `build.cmd debug`                              | ⚠️ 需匹配版本的 Debug SDK |
| **清理构建**     | `build.cmd clean`                              | 删除 build 目录           |
| **重新构建**     | `build.cmd rebuild`                            | 清理后重新编译            |
| **运行程序**     | `.\build\release\MeetingApp.exe`               | 直接执行                  |
| **升级 SDK**     | 编辑 `LIVEKIT_VERSION` → `build.cmd rebuild`   | 自动下载新版              |
| **查看详细编译** | `cmake --build build/release --verbose`        | 显示编译命令              |
| **生成 VS 项目** | `cmake -B build/vs -G "Visual Studio 17 2022"` | 用于 IDE 调试             |
| **依赖检查**     | `dumpbin /DEPENDENTS *.exe`                    | 查看 DLL 依赖             |

### 工作流建议

```
日常开发流程:
1. 修改代码
2. build.cmd                  ← 增量编译，很快
3. .\build\release\MeetingApp.exe
4. 重复 1-3

遇到问题时:
1. build.cmd clean
2. build.cmd rebuild          ← 完全重新构建
3. 如果仍然失败，检查 SDK 版本兼容性

发布版本时:
1. build.cmd release
2. windeployqt.exe ...        ← 收集 Qt DLL
3. 复制到 dist/ 目录
4. 打包成安装程序
```

### ⚠️ 注意事项

1. **Debug SDK 版本问题**  
   目前 `extend/livekit-sdk-windows-x64-debug/` 是旧版本，与新的 Release SDK (v0.2.7) 可能不兼容。  
   **解决方案**：
   - 推荐只使用 Release 构建
   - 或手动下载匹配版本的 Debug SDK 并替换

2. **始终使用 build.cmd**  
   不要直接运行 `cmake`，因为需要初始化 MSVC 环境

3. **SDK 升级后测试**  
   升级 LiveKit SDK 版本后，需要完整测试所有功能，确保 API 兼容

---

**📚 参考资源**

- CMake 官方文档: https://cmake.org/documentation/
- Ninja 构建系统: https://ninja-build.org/
- MSVC 编译器选项: https://learn.microsoft.com/en-us/cpp/build/reference/compiler-options
- LiveKit C++ SDK: https://github.com/livekit/client-sdk-cpp
