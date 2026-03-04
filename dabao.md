# MeetingApp 打包发布指南

本文档介绍如何将 MeetingApp 打包成可分发的 `.exe` 程序或文件夹，让其他用户可以直接运行。

---

## 一、准备工作

### 1. 使用 Release 模式编译

首先确保你的程序是以 **Release** 模式编译的（而非 Debug），这样：

- 程序运行更快
- 体积更小
- 不需要 Debug 版本的运行时库

在 Qt Creator 中：

1. 点击左下角的构建模式选择器
2. 选择 `Release` 模式
3. 点击 **构建** → **重新构建项目**

编译完成后，你的 `.exe` 文件位于：

```
build/Desktop_Qt_6_5_3_MSVC2019_64bit-Release/MeetingApp.exe
```

---

## 二、创建发布文件夹

### 1. 创建目录结构

```powershell
# 在桌面或其他位置创建发布目录
mkdir C:\MeetingApp_Release
```

### 2. 复制主程序

```powershell
# 复制编译好的exe文件
copy "C:\Users\86158\Desktop\meeting\build\Desktop_Qt_6_5_3_MSVC2019_64bit-Release\MeetingApp.exe" "C:\MeetingApp_Release\"
```

---

## 三、部署 Qt 依赖（核心步骤）

Qt 程序需要大量 DLL 文件才能运行。使用 Qt 自带的 `windeployqt` 工具自动收集：

### 1. 打开 Qt 命令行

从开始菜单打开：**Qt 6.5.3 (MSVC 2019 64-bit)** 命令行工具

或者手动设置环境变量后使用普通命令行：

```powershell
# 设置Qt路径（根据你的实际安装位置调整）
$env:PATH = "C:\Qt\6.5.3\msvc2019_64\bin;" + $env:PATH
```

### 2. 运行 windeployqt

```powershell
# 切换到发布目录
cd C:\MeetingApp_Release

# 运行部署工具（包含QML支持）
windeployqt.exe --release --qmldir "C:\Users\86158\Desktop\meeting\resources\qml" MeetingApp.exe
```

**参数说明：**

- `--release`：使用 Release 版本的 Qt 库
- `--qmldir`：指定 QML 文件目录，自动检测所需的 QML 模块

---

## 四、复制 LiveKit SDK 依赖

你的项目使用了 LiveKit SDK，需要手动复制相关 DLL：

```powershell
# 复制 LiveKit SDK 的 DLL 文件 (Release 模式只需这两个)
copy "C:\Users\86158\Desktop\meeting\extend\livekit-sdk-windows\bin\livekit_ffi.dll" "C:\MeetingApp_Release\"
copy "C:\Users\86158\Desktop\meeting\extend\livekit-sdk-windows\bin\livekit.dll" "C:\MeetingApp_Release\"
```

---

## 五、安装 Visual C++ 运行时

用户电脑上需要安装 **Visual C++ Redistributable**，否则会提示缺少 `vcruntime140.dll` 等文件。

### 方案一：让用户自行安装（推荐）

提供下载链接让用户安装：

- [Microsoft Visual C++ 2015-2022 Redistributable (x64)](https://aka.ms/vs/17/release/vc_redist.x64.exe)

### 方案二：将运行时打包进去

从你的系统复制这些文件到发布目录（通常在 `C:\Windows\System32\`）：

- `vcruntime140.dll`
- `vcruntime140_1.dll`
- `msvcp140.dll`
- `msvcp140_1.dll`
- `msvcp140_2.dll`

---

## 六、最终目录结构

打包完成后，你的 `MeetingApp_Release` 文件夹应该类似这样：

```
MeetingApp_Release/
├── MeetingApp.exe              # 主程序
├── Qt6Core.dll                 # Qt核心库
├── Qt6Gui.dll                  # Qt GUI库
├── Qt6Quick.dll                # Qt Quick库
├── Qt6QuickControls2.dll       # Qt控件库
├── Qt6Multimedia.dll           # 多媒体库
├── Qt6Network.dll              # 网络库
├── Qt6WebSockets.dll           # WebSocket库
├── Qt6Concurrent.dll           # 并发库
├── livekit.dll                 # LiveKit 核心库 (内嵌abseil等依赖)
├── livekit_ffi.dll             # LiveKit SDK FFI接口库
├── platforms/                  # Qt平台插件
│   └── qwindows.dll
├── imageformats/               # 图像格式插件
├── multimedia/                 # 多媒体插件
├── qml/                        # QML模块
│   ├── QtQuick/
│   ├── QtQuick.Controls/
│   └── ...
└── ...
```

---

## 七、测试发布包

**重要：在打包好的文件夹能运行之前，务必测试！**

### 测试步骤：

1. **在另一台电脑上测试**（最佳）

   - 将整个文件夹复制到没有安装 Qt 的电脑
   - 双击 `MeetingApp.exe` 运行

2. **在本机模拟测试**
   - 临时重命名或移动你的 Qt 安装目录
   - 运行打包好的程序
   - 测试完后恢复 Qt 目录

### 常见问题排查：

如果程序无法启动，通常是缺少 DLL，可以：

1. 使用 [Dependencies](https://github.com/lucasg/Dependencies) 工具分析缺少哪些 DLL
2. 查看 Windows 事件查看器中的应用程序错误日志

---

## 八、进阶：创建安装程序（可选）

如果想要更专业的分发方式，可以使用安装程序打包工具：

### 推荐工具：

1. **Inno Setup**（免费，推荐）

   - 下载：https://jrsoftware.org/isinfo.php
   - 特点：轻量、脚本简单、生成单个 exe 安装包

2. **NSIS**（免费）

   - 下载：https://nsis.sourceforge.io/
   - 特点：功能强大、高度可定制

3. **Qt Installer Framework**（Qt 官方）
   - 特点：跨平台、支持在线更新

### Inno Setup 简单示例脚本：

```iss
[Setup]
AppName=MeetingApp
AppVersion=1.0.0
DefaultDirName={pf}\MeetingApp
DefaultGroupName=MeetingApp
OutputBaseFilename=MeetingApp_Setup
Compression=lzma2
SolidCompression=yes

[Files]
; 复制所有文件
Source: "C:\MeetingApp_Release\*"; DestDir: "{app}"; Flags: recursesubdirs

[Icons]
; 创建桌面快捷方式
Name: "{commondesktop}\MeetingApp"; Filename: "{app}\MeetingApp.exe"
; 创建开始菜单快捷方式
Name: "{group}\MeetingApp"; Filename: "{app}\MeetingApp.exe"

[Run]
; 安装完成后运行程序（可选）
Filename: "{app}\MeetingApp.exe"; Description: "启动 MeetingApp"; Flags: postinstall nowait
```

---

## 九、一键打包脚本

为了方便，可以创建一个 PowerShell 脚本自动完成上述步骤：

```powershell
# deploy.ps1 - MeetingApp 打包脚本

# 配置路径（根据实际情况修改）
$ProjectDir = "C:\Users\86158\Desktop\meeting"
$BuildDir = "$ProjectDir\build\Desktop_Qt_6_5_3_MSVC2019_64bit-Release"
$QtDir = "C:\Qt\6.5.3\msvc2019_64"  # 修改为你的Qt安装路径
$OutputDir = "C:\MeetingApp_Release"

# 1. 创建输出目录
if (Test-Path $OutputDir) {
    Remove-Item -Recurse -Force $OutputDir
}
New-Item -ItemType Directory -Path $OutputDir

# 2. 复制主程序
Copy-Item "$BuildDir\MeetingApp.exe" $OutputDir

# 3. 运行 windeployqt
& "$QtDir\bin\windeployqt.exe" --release --qmldir "$ProjectDir\resources\qml" "$OutputDir\MeetingApp.exe"

# 4. 复制 LiveKit DLL
$LiveKitDlls = @(
    "livekit.dll",
    "livekit_ffi.dll"
)
foreach ($dll in $LiveKitDlls) {
    # 路径根据实际LiveKit SDK存放位置获取
    Copy-Item "$ProjectDir\build\_deps\livekit-build\$dll" $OutputDir
}

Write-Host "打包完成！输出目录: $OutputDir" -ForegroundColor Green
```

运行脚本：

```powershell
powershell -ExecutionPolicy Bypass -File deploy.ps1
```

---

## 十、发布清单

在分发给用户前，确认以下事项：

- [ ] 使用 Release 模式编译
- [ ] 运行 `windeployqt` 部署 Qt 依赖
- [ ] 复制 LiveKit SDK 所有 DLL
- [ ] 在干净的电脑上测试运行
- [ ] 提醒用户安装 VC++ 运行时（或打包进去）
- [ ] 准备简单的使用说明

---

## 常见错误及解决方案

| 错误信息              | 原因               | 解决方案                     |
| --------------------- | ------------------ | ---------------------------- |
| 缺少 Qt6Core.dll      | Qt 库未部署        | 运行 windeployqt             |
| 缺少 vcruntime140.dll | VC++运行时未安装   | 安装 VC++ Redistributable    |
| 缺少 livekit_ffi.dll  | LiveKit SDK 未复制 | 复制 LiveKit DLL 文件        |
| QML module not found  | QML 模块未部署     | windeployqt 加 --qmldir 参数 |
| 程序闪退无提示        | 可能缺少插件       | 用 Dependencies 工具检查     |

---

祝你打包顺利！🎉
