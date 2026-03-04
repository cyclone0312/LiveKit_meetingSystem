# MeetingApp 构建与 CI 完整知识讲解

> 本文档覆盖三块内容：
>
> 1. **CMake 动态拉取依赖**（`FetchLiveKit.cmake` / `FetchGoogleTest.cmake` 的原理）
> 2. **`build.cmd` 本地构建脚本**（每一步在做什么）
> 3. **`git push` 与 GitHub Actions CI/CD**（代码推送到触发 Release 的全流程）

---

## 第一部分：CMake 动态拉取依赖

### 1.1 为什么要"动态拉取"？

传统做法是把第三方库的 `.lib`/`.dll`/`.h` 直接放进仓库，缺点明显：

- 二进制文件体积巨大，git 仓库膨胀
- 换平台/版本需要手动替换文件
- 不同开发者本地路径不同，CMake 找不到库

**动态拉取**的思路是：只把**下载脚本**（`.cmake` 文件）放进仓库，
CMake 在 configure 阶段自动从网络下载并解压，整个过程对使用者透明。

---

### 1.2 `cmake/FetchLiveKit.cmake` 完整工作流

```
cmake -B build   ← 触发 configure 阶段
       │
       ▼
include(cmake/FetchLiveKit.cmake)
       │
       ├─ 1. 检测平台 (Windows/Linux/macOS) 和架构 (x64/arm64)
       │        └─ 组装下载 URL 和 SDK 目录名
       │
       ├─ 2. 检查 stamp 文件是否存在
       │        ├─ 存在 → 跳过下载（已缓存）
       │        └─ 不存在 → 执行下载流程
       │                  ├─ file(DOWNLOAD ...) 下载压缩包
       │                  ├─ execute_process(tar xzf ...) 解压
       │                  └─ file(WRITE stamp) 写入标记文件
       │
       └─ 3. 设置变量 & 创建 Imported Target
                ├─ LIVEKIT_INCLUDE_DIR / LIVEKIT_LIB_DIR / LIVEKIT_BIN_DIR
                └─ add_library(LiveKit::livekit SHARED IMPORTED)
```

#### 关键 CMake 命令解析

| 命令                                               | 作用                                          |
| -------------------------------------------------- | --------------------------------------------- |
| `file(DOWNLOAD url dest SHOW_PROGRESS STATUS ...)` | 在 configure 阶段从 URL 下载文件              |
| `execute_process(COMMAND cmake -E tar xzf ...)`    | 跨平台解压（cmake -E 是 CMake 内置工具集）    |
| `file(WRITE path content)`                         | 写入 stamp 文件，下次 configure 跳过下载      |
| `file(GLOB var pattern)`                           | 列出匹配的文件/目录，用于探测解压后的目录结构 |
| `file(RENAME src dst)`                             | 移动目录到最终位置                            |

#### 什么是 Stamp 文件？

```bash
third_party/livekit-sdk-windows-x64-0.3.1/.livekit-0.3.1.stamp
```

这只是一个普通文本文件，内容无所谓。CMake 每次 configure 都检查：

- **文件存在** → SDK 已下载完毕，直接跳到"设置路径"步骤
- **文件不存在** → 重新下载解压

这样即使你删了 `build/` 目录重新 configure，也不会重新下载 SDK。

---

### 1.3 Imported Target 是什么？

传统链接方式（脆弱，依赖硬编码路径）：

```cmake
# 旧方式：直接写死路径
target_link_libraries(MyApp PRIVATE "${SDK_DIR}/lib/livekit.lib")
target_include_directories(MyApp PRIVATE "${SDK_DIR}/include")
```

**Imported Target** 方式（现代 CMake 推荐）：

```cmake
# 创建一个"虚拟目标"，把所有信息封装进去
add_library(LiveKit::livekit SHARED IMPORTED)
set_target_properties(LiveKit::livekit PROPERTIES
    IMPORTED_IMPLIB   "path/to/livekit.lib"   # Windows 导入库
    IMPORTED_LOCATION "path/to/livekit.dll"   # 运行时 DLL
    INTERFACE_INCLUDE_DIRECTORIES "path/to/include"
)

# 使用时只需一行，头文件路径自动传递
target_link_libraries(MyApp PRIVATE LiveKit::livekit)
```

优点：

- 头文件路径自动通过 `INTERFACE_INCLUDE_DIRECTORIES` 传播
- 支持 `IMPORTED_CONFIGURATIONS`（Debug/Release 分别配置）
- 符合现代 CMake 的 **target-based** 设计理念

---

### 1.4 `cmake/FetchGoogleTest.cmake` — FetchContent 方案

GoogleTest 用的是另一种机制：`FetchContent`，这是 CMake 3.11+ 内置模块。

```cmake
include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
    GIT_SHALLOW    TRUE          # 只克隆最新一个 commit，节省流量
)

FetchContent_MakeAvailable(googletest)
# 执行后 GTest::gtest / GTest::gtest_main 目标自动可用
```

**FetchContent vs 手动下载的区别：**

|          | `FetchContent`             | 手动 `file(DOWNLOAD)`             |
| -------- | -------------------------- | --------------------------------- |
| 适用对象 | 有 CMakeLists.txt 的源码库 | 预编译的二进制 SDK/压缩包         |
| 构建方式 | 把库的源码一起编译进来     | 直接链接预编译的 .lib/.so         |
| 缓存位置 | `build/_deps/`             | 自定义（本项目用 `third_party/`） |

---

### 1.5 macOS arm64 问题根本原因

`macos-latest` GitHub Actions Runner 从 2024 年起是 **Apple Silicon M1 (arm64)**。
LiveKit 官方 Release 页面中，macOS 只发布了 `livekit-sdk-macos-arm64-0.3.1.tar.gz`，
**不存在** `livekit-sdk-macos-x64-0.3.1.tar.gz`，所以下载报 HTTP 404。

修复方法：在 `FetchLiveKit.cmake` 中，检测到 macOS 时将默认架构设为 `arm64`：

```cmake
if(NOT DEFINED LINKS_SDK_ARCH)
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(LINKS_SDK_ARCH "arm64")   # ← macOS 默认 arm64
    else()
        set(LINKS_SDK_ARCH "x64")
    endif()
endif()
```

---

## 第二部分：`build.cmd` 本地构建脚本详解

`build.cmd` 是 Windows 批处理脚本，作用是把"需要执行的一系列命令"自动化。

### 2.1 整体结构

```
build.cmd
│
├─ 路径配置区（VCVARSALL / QT_DIR / NINJA_PATH / CMAKE_PATH）
├─ 参数解析（release / debug / test / clean / rebuild）
│
├─ :build  ─── 主构建流程（分 3 步）
├─ :test   ─── 构建 + 跑测试（分 4 步）
├─ :clean  ─── 删除构建目录
└─ :rebuild ── 先 clean 再 build
```

### 2.2 关键步骤逐行解析

#### Step 1：初始化 MSVC 环境

```bat
call "%VCVARSALL%" x64 >nul 2>&1
```

`vcvarsall.bat` 是 Visual Studio 提供的环境初始化脚本。调用它之后，`cl.exe`（MSVC 编译器）、
`link.exe`（链接器）、Windows SDK 头文件等才会被加入 `PATH` 和环境变量中。

- `x64` 参数表示编译目标架构是 64 位
- `>nul 2>&1` 把所有输出丢弃（避免刷屏）
- 如果不调用这个脚本，`cmake` 找不到 `cl.exe` 会直接报错

#### Step 2：CMake configure

```bat
cmake -S "%PROJECT_DIR%" -B "%BUILD_DIR%" ^
    -G Ninja ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DCMAKE_PREFIX_PATH="%QT_DIR%" ^
    -DCMAKE_MAKE_PROGRAM="%NINJA_PATH%\ninja.exe" ^
    -DCMAKE_C_COMPILER="cl.exe" ^
    -DCMAKE_CXX_COMPILER="cl.exe"
```

| 参数                     | 含义                                          |
| ------------------------ | --------------------------------------------- |
| `-S`                     | 源码目录（CMakeLists.txt 所在位置）           |
| `-B`                     | 构建目录（生成 build.ninja 等文件）           |
| `-G Ninja`               | 使用 Ninja 作为构建系统（比 MSBuild 快很多）  |
| `-DCMAKE_BUILD_TYPE`     | Release 或 Debug（影响优化级别、调试符号）    |
| `-DCMAKE_PREFIX_PATH`    | Qt 安装目录，`find_package(Qt6)` 从这里找     |
| `-DCMAKE_C/CXX_COMPILER` | 明确指定用 MSVC 的 cl.exe 而不是 MinGW 的 gcc |

> `configure` 阶段不编译任何代码，只是"读 CMakeLists.txt → 生成构建文件（build.ninja）"。
> 此时也会触发 `FetchLiveKit.cmake` 下载 SDK。

#### Step 3：编译

```bat
cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% -j %NUMBER_OF_PROCESSORS%
```

- `cmake --build` 调用底层构建系统（这里是 Ninja）
- `-j %NUMBER_OF_PROCESSORS%` 并行编译，使用 CPU 全部核心

#### 为什么分 configure 和 build 两步？

- `configure` 只需要运行一次（或改了 CMakeLists.txt 才重跑）
- `build` 增量编译：只有修改过的 `.cpp` 才重新编译
- 大型项目里这能节省大量时间

---

## 第三部分：`git push` 与 GitHub Actions 全流程

### 3.1 本地操作：从修改到推送

```
编辑文件
  │
  ▼
git add -A               ← 暂存区（Staging Area）：告诉 git "我要提交这些文件"
  │
  ▼
git commit -m "message"  ← 将暂存区的变更保存为一个"快照"（commit）
  │
  ▼
git push origin main     ← 把本地新 commit 上传到 GitHub 远程仓库
```

#### 暂存区（Staging Area）是什么？

Git 的三层模型：

```
工作区（你看到的文件）
    │  git add
    ▼
暂存区（index/staging）
    │  git commit
    ▼
本地仓库（.git/）
    │  git push
    ▼
远程仓库（GitHub）
```

暂存区的意义：允许你只提交部分修改过的文件，而不是一次性提交所有变更。

#### commit 是什么？

每个 commit 是项目在某一时刻的完整快照，包含：

- 所有文件的内容（通过 SHA-1 哈希存储）
- 作者、时间、提交信息
- 指向父 commit 的指针（形成链表）

```
HEAD → main → [commit f0aff3d] → [commit b53ddfa] → ...
```

---

### 3.2 GitHub Actions 触发机制

```yaml
on:
  push:
    branches: [main] # ← push 到 main 分支时触发
  pull_request:
    branches: [main] # ← 向 main 发起 PR 时触发
```

每次 `git push origin main` 后，GitHub 检测到事件，自动：

1. 在云端创建一个全新的虚拟机（Runner）
2. 按 `.github/workflows/ci.yml` 的定义执行每个 step
3. 结果显示在 GitHub 仓库的 Actions 标签页

---

### 3.3 `ci.yml` 三平台 Job 详解

#### 并行执行

```yaml
jobs:
  build-windows: ──┐
  build-linux: ──┼── 三个 job 同时在不同机器上并行运行
  build-macos: ──┘
```

Jobs 之间默认并行，总时间取决于最慢的那个。

#### actions/cache — SDK 缓存加速

```yaml
- uses: actions/cache@v4
  with:
    path: third_party # 缓存的目录
    key: livekit-sdk-windows-x64-${{ hashFiles('cmake/FetchLiveKit.cmake') }}
```

工作原理：

1. **第一次运行**：`third_party/` 不存在于缓存 → CMake 下载 SDK → job 结束后 Actions 把 `third_party/` 打包存入缓存
2. **后续运行**：key 匹配 → 直接从缓存恢复 `third_party/`，CMake 检测到 stamp 文件存在，跳过下载

`hashFiles('cmake/FetchLiveKit.cmake')` 的意义：SDK 脚本内容改变（比如升级版本）时，key 自动变化，缓存失效并重新下载。

#### 三平台的不同点

|            | Windows                          | Linux                           | macOS                        |
| ---------- | -------------------------------- | ------------------------------- | ---------------------------- |
| Runner     | `windows-latest`                 | `ubuntu-latest`                 | `macos-latest`               |
| 编译器配置 | `ilammy/msvc-dev-cmd` + `cl.exe` | 系统 GCC（无需额外配置）        | 系统 Clang（无需额外配置）   |
| 额外安装   | —                                | `apt-get` 安装 X11/GStreamer 等 | `brew install ninja`         |
| SDK 架构   | x64                              | x64                             | **arm64**（Apple Silicon）   |
| 并行参数   | `$env:NUMBER_OF_PROCESSORS`      | `$(nproc)`                      | `$(sysctl -n hw.logicalcpu)` |

---

### 3.4 `release.yml` — Tag 触发的 Release 构建

#### 触发条件：Tag vs Branch

```yaml
on:
  push:
    tags:
      - "v*" # 匹配 v1.0.0、v2.3.1-beta 等
```

- **Branch push**：代码的每次日常提交，触发 CI 检查
- **Tag push**：发布某个版本时才触发，代表"这是一个正式版本"

创建并推送 Tag：

```bash
git tag v1.0.0           # 在当前 commit 上打标签
git push origin v1.0.0   # 推送标签到 GitHub（不会自动随 push branch 推送）
```

#### windeployqt — Qt 运行时收集

Qt 应用不能直接分发单个 `.exe`，用户机器上通常没有 Qt 运行时。
`windeployqt` 是 Qt 提供的工具，用于：

1. 扫描 `MeetingApp.exe` 的依赖
2. 把所需的 Qt DLL（`Qt6Core.dll`、`Qt6Quick.dll` 等）复制到同目录
3. 扫描 QML 导入，复制对应的 QML 模块到 `qml/` 子目录

```bat
windeployqt --qmldir resources/qml MeetingApp.exe
```

执行后目录结构类似：

```
deploy/
├── MeetingApp.exe
├── Qt6Core.dll
├── Qt6Quick.dll
├── livekit.dll
├── livekit_ffi.dll
└── qml/
    ├── QtQuick/
    └── QtQuick/Controls/
```

#### softprops/action-gh-release — 自动创建 Release

```yaml
- uses: softprops/action-gh-release@v2
  with:
    tag_name: v1.0.0
    files: build/MeetingApp-windows-x64-v1.0.0.zip
```

这个 Action 会：

1. 在 GitHub 仓库创建一个 Release 页面（类似 GitHub 的"发布"功能手动操作）
2. 将打包好的 zip 文件作为下载附件上传

`prerelease: ${{ contains(github.ref_name, '-') }}` 的含义：

- `v1.0.0` → 正式版（prerelease = false）
- `v1.0.0-beta` / `v1.0.0-rc1` → 预发布版（prerelease = true）

---

### 3.5 `concurrency` — 防止 CI 堆积

```yaml
concurrency:
  group: ${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true
```

场景：你连续快速推送了 3 个 commit，GitHub Actions 会排队启动 3 次 CI。
开启 `cancel-in-progress` 后：只运行最新的那次，旧的自动取消，节省 Actions 用量。

---

## 总结：完整的开发 → 发布链路

```
本地开发
  │
  ├─ 日常编写代码 → build.cmd release 验证构建
  │                    └─ CMake configure（FetchLiveKit 自动拉 SDK）
  │                         └─ Ninja 增量编译
  │
  ├─ git push origin main
  │     └─ GitHub Actions 触发 CI Build
  │           ├─ build-windows (MSVC)
  │           ├─ build-linux   (GCC)    ← 并行运行
  │           └─ build-macos   (Clang)
  │
  └─ git tag v1.0.0 && git push origin v1.0.0
        └─ GitHub Actions 触发 Release
              ├─ MSVC Release 构建
              ├─ windeployqt 收集 Qt 运行时
              ├─ 打包为 zip
              └─ 发布到 GitHub Releases 页面（对外可下载）
```
