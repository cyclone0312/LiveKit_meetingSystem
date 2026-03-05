🚀 跨平台智能视频会议系统 (Smart Video Meeting System)
📖 项目简介
本项目是一个基于 SFU 架构 的跨平台智能视频会议系统。项目不仅实现了原生桌面端与 Web 端的低延迟音视频异构互通，还深度集成了 阿里云百炼大模型生态 (DashScope)，提供实时语音转录与 AI 会议纪要生成功能。

系统采用现代 C++ (C++17) 与 Qt 6 构建客户端，底层深度定制 WebRTC 媒体流，通过 GitHub Actions Matrix 实现多系统全自动 CI/CD 流水线，后端采用 Node.js + Docker 容器化微服务部署，构建了一个高可用、现代化的企业级远程协作平台。

✨ 核心亮点
🌐 跨平台通信与底层媒体重构

基于 LiveKit SDK 打通了原生桌面端与 Web 端的推拉流互通。

重写 VideoSinkInterface 接口对接远端 I420 视频数据，并结合 Qt 6 重构 QVideoFrame 内存映射逻辑，实现多路高清视频流的低延迟丝滑渲染。

🧠 云原生 AI 智能会议助手

独立实现基于音频切片的语音识别（STT）传输引擎。

深度对接阿里云百炼 API（通义千问 LLM），实现会中实时问答、跨房间状态同步及自动化“AI 会议纪要”提炼，极大提升会议后处理效率。

🎧 企业级音频调优与多线程架构

客户端采用多线程异步架构，彻底解耦 UI 渲染与网络通信，防止界面阻塞。

深度集成 WebRTC 核心的 APM（音频处理模块），实现了企业级的回声消除(AEC)、自动增益控制(AGC)及降噪(ANS)。

⚡ 现代化 CI/CD 与动态依赖管理

摒弃传统的二进制库硬编码入库，编写自定义 CMake 脚本 (FetchLiveKitSDK.cmake)，实现第三方依赖在多系统下的动态嗅探、按需拉取与自动链接。

部署 GitHub Actions 矩阵流水线，实现 Windows / Linux / macOS 三大平台的源码级云端全自动持续集成。

🛠️ 技术栈
客户端引擎: C++17, Qt 6.8 (QML)

音视频与网络: WebRTC, LiveKit C++ SDK, TCP/UDP

AI 与云端服务: 阿里云百炼 API (DashScope), Node.js (Express), MySQL, Docker

工程化与构建: CMake, Ninja, GitHub Actions CI/CD

📁 核心架构概览
项目遵循现代化的模块解耦设计（MVVM 架构融合）：

MeetingController (核心中枢)：调度多线程工作，统筹 UI 响应与后台网络通信。

LiveKitManager (SFU 交互层)：封装房间逻辑与信令交互，处理服务端下发的跨端事件回调与 Token 鉴权。

MediaCapture (硬核媒体层)：直接接管底层音视频硬件设备，处理 Qt Video/Audio 原始帧与 WebRTC 轨道数据之间的格式转换与内存拷贝。

AI Assistant (智能体模块)：管理音频转码上传，调用 REST API 与阿里云百炼进行实时流式交互。

🚀 快速构建与运行指南
得益于项目内置的动态依赖拉取模块与自动化构建脚本，您可以极速完成项目的编译与运行。

前提条件：已安装 CMake (3.15+) 及 Qt 6 开发环境。

1. 克隆项目：

Bash
git clone https://github.com/cyclone0312/LiveKit_meetingSystem.git
cd LiveKit_meetingSystem
2. Windows 环境一键构建与运行 (推荐)：
项目中提供了编写好的批处理脚本，直接在终端执行以下命令，即可自动完成 CMake 配置、Ninja 编译并启动程序：

DOS
.\build.cmd && .\build\release\MeetingApp.exe
3. Linux / macOS 手动跨平台构建：

Bash
mkdir build && cd build
# CMake 将自动嗅探当前系统环境，动态下载对应的跨平台 LiveKit SDK
cmake .. 
cmake --build . --config Release

## 📄 更多技术文档
项目中包含了不同维度的开发手册：
- [架构概览及交互调用关系图](./docs/summary.md)
- [视频流输入输出工作原理深究](./docs/video_module_explanation.md)
- [AI 模块技术文档](./docs/AI_模块技术文档.md)
- [扩展功能集成文档](./docs/expend.md)
- [自动化构建与 CI/CD 指南](./docs/BUILD_AND_CI_GUIDE.md)
- [项目构建指南](./docs/BUILD_GUIDE.md)
- [CMake 手动构建指南](./docs/cmake_manual_build_guide.md)
- [打包与发布指南](./docs/dabao.md)
- [CLI 终端开发命令参考](./docs/CLI_REFERENCE.md)
- [Git PR 工作流团队协作指南](./docs/github_pr_workflow.md)
