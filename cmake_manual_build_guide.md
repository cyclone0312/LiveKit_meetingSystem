# MeetingApp CMake 手动构建指南

本文档将 `build.cmd` 脚本中的自动构建过程拆解为手动在终端中执行的每一个步骤。通过在纯命令行（`cmd`）中一行行执行这些代码，可以更深入地理解现代 C++ (CMake + Qt) 工程的底层构建逻辑。

## ⚠️ 准备工作
请打开一个全新的 **Command Prompt (`cmd`)** 终端。
*注意：请勿使用 PowerShell，因为运行 `.bat` 脚本设置的环境变量无法在 PowerShell 的后续命令中完美继承。*

确保当前工作目录位于项目根目录：
```cmd
cd /d c:\Users\86158\Desktop\meeting
```

---

## 🎯 第一步：初始化 MSVC C++ 编译器环境（属于构建前的“找人”准备阶段）

**执行命令：**
```cmd
call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x64
```

**原理说明：**
你可以把这步理解为**“招聘并配置底层干活的工人”**。
Windows 系统的全局环境变量中默认并不包含 C++ 编译器（`cl.exe`）和链接器（`link.exe`）的路径。
这条 `call` 命令调用了 Visual Studio 自带的环境配置脚本，参数 `x64` 表示要构建 64 位程序。执行后，它会在当前终端临时注入相关构建所需的环境变量（如 `INCLUDE`, `LIB`, `PATH` 等），相当于为当前终端配置了完整的 C++ 开发环境。没有这一步，后续工具会报错“找不到编译器”。

---

## 🎯 第二步：配置 CMake 和 Ninja 环境变量（属于构建前的“找管理”准备阶段）

**执行命令：**
```cmd
set "PATH=D:\qt\Tools\CMake_64\bin;D:\qt6.4\Tools\Ninja;%PATH%"
```

**原理说明：**
你可以把这步理解为**“招聘项目经理（CMake）和包工头（Ninja）”**。
本项目的主要构建流程依赖以下两个核心工具：
1. **CMake**（项目经理）：构建配置系统，负责读取 `CMakeLists.txt`，寻找 Qt 等外部依赖库，并生成具体打工人（Ninja）所需的排班表。
2. **Ninja**（包工头）：低级构建系统，拿着排班表，以极快的速度调度编译器高并发执行真正的编译和链接工作。
此命令将 Qt 安装目录中自带的特定版本 CMake 和 Ninja 的路径放在系统 `PATH` 的最前面，确保后续使用的是正确的工具版本。

---

## 🎯 第三步：生成构建工程文件（CMake Configure 阶段，属于“生成图纸”阶段）

**执行命令：**
*(你可以带上 `^` 符号将这整块直接复制并运行)*
```cmd
cmake -S . -B build/release_manual ^
    -G Ninja ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_PREFIX_PATH="D:\qt6.4\6.8.3\msvc2022_64" ^
    -DCMAKE_MAKE_PROGRAM="D:\qt6.4\Tools\Ninja\ninja.exe" ^
    -DCMAKE_C_COMPILER="cl.exe" ^
    -DCMAKE_CXX_COMPILER="cl.exe"
```

**原理说明：**
这一步绝对**不是**在把代码变成软件，而是**项目经理（CMake）在画图纸和排班**。
执行完成后，CMake 会在项目目录下创建 `build/release_manual` 文件夹，并在其中生成 `.ninja` 构建配置清单（可以理解为极其详细的施工排班表）。
- `-S .` / `-B build/release_manual`：指定代码图纸目录为当前目录（`.`），产物输出目录新建一个叫 `release_manual` 的文件夹。
- `-G Ninja`：指定包工头为 Ninja。放弃生成笨重的 Visual Studio 工程（`.sln`），转而生成轻量级的 `build.ninja` 排班表。
- `-DCMAKE_BUILD_TYPE=Release`：指定最终要造出来的产品类型（带优化、无调试信息的正式发行版）。
- `-DCMAKE_PREFIX_PATH`：**寻找特殊建材供应商（Qt）。** 告诉 CMake 去哪里寻找 Qt6 的核心库以及配置文件，以便后续处理特殊的 Qt 代码（moc、uic）。
- 最后的三个参数：指定好确切的工具人和编译器，防止包工头找错人。

---

## 🎯 第四步：执行正式代码编译与链接（CMake Build 阶段）

**执行命令：**
```cmd
cmake --build build/release_manual --config Release -j 16
```

**原理说明（对应 C++ 编译的四大阶段）：**
此阶段 CMake 仅作为命令的统一入口，实际的编译工作移交给了基层的 **Ninja**。Ninja 会根据生成的 `build.ninja` 文件，调度底层的编译器（`cl.exe`）和链接器（`link.exe`）开足马力干活。

在这个真正的构建过程中，你的代码经历了 C++ 经典的**四大阶段**：

1. **预处理 (Preprocessing) & Qt 特有代码生成**
   - **Qt 预处理**：在标准 C++ 编译之前，Ninja 会先调用 Qt 的专属工具（如调用 `moc.exe` 处理包含信号与槽的头文件，调用 `uic.exe` 将 `.ui` 界面文件转换为 C++ 头文件），将 Qt 特有的语法还原成标准的 C++ 代码。
   - **标准预处理**：随后，微软编译器 `cl.exe` 开始介入。它会处理代码中所有的宏定义（`#define`）、展开所有引入的头文件内容（`#include`），并剔除那些条件编译（`#ifdef`）未命中的无用代码。
   
2. **编译 (Compilation)**
   - `cl.exe` 会对经过预处理得到的庞大纯 C++ 代码进行词法、语法和语义分析。在此阶段，因为我们配置了 `Release` 模式，编译器会对代码逻辑进行各种深度优化，并最终将其转化为底层的**汇编指令 (Assembly instructions)**。

3. **汇编 (Assembly)**
   - 紧接着，汇编器会将这些汇编指令进一步翻译成计算机硬件能够直接读懂的二进制机器码，由此为你项目里的每一个 `.cpp` 源文件生成一个对应的**目标文件（Object File，Windows 下后缀为 `.obj`）**。
   *(此时终端上飞速滚动的 `[1/65] Building CXX object...` 对应的就是上面这前三个阶段。)*

4. **链接 (Linking)**
   - 当所有的 `.cpp` 文件都变成了碎片化的 `.obj` 文件后，构建进入最后一步。Ninja 会呼叫微软链接器（`link.exe`），将这堆零散的 `.obj` 文件，与系统底层的 C++ 运行库以及你挂载的 Qt 核心动态链接库（如 `Qt6Core.lib` 等），像拼图一样全部关联组装在一起。只要有任何一个函数找不到实现代码，这一步就会报大家最怕的 `LNK2019: 无法解析的外部符号` 错误。
   - 拼装成功后，最终诞生出完整的可执行程序 `MeetingApp.exe`。
