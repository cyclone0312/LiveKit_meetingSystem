# 视频会议系统 (Video Meeting System)

## 📖 项目简介
本项目是一个基于 C++ 和 Qt (QML) 开发，结合 LiveKit SDK 的实时视频会议客户端应用。项目基于现代 C++ 设计了清晰的架构，实现了高质量的多人实时音视频通话功能，包含了底层音视频媒体的采集与格式转换、LiveKit 房间的连接交互、会议状态流转控制，以及现代化的 Qt 界面展示。

## ✨ 核心特性
- **多人音视频互动**：基于 LiveKit 强大的 WebRTC 架构底座，支持低延迟的多人在线会议。
- **现代化 UI 设计**：使用 Qt Quick (QML) 实现的高级用户界面，包含自适应参会者网格布局、多功能控制面板与实时聊天区域。
- **灵活的媒体控制**：定制化的媒体抽象层，支持本地摄像头与麦克风的一键启停控制，并将 Qt 环境下的多媒体流与 LiveKit 底层轨道完美整合。
- **双向实时通信**：内置了实时的文字聊天广播面板与会议参与者状态同步机制。
- **跨平台基础**：基于 CMake 构建体系、跨平台的 Qt 框架与 LiveKit C++ SDK 设计，为后续跨平台跨端扩展打下了良好的基础。

## 🛠️ 技术栈
- **核心开发**: C++17 及以上
- **UI 框架**: Qt 6.x (QML)
- **音视频底座**: LiveKit SDK (C++ SDK)
- **构建系统**: CMake, Ninja, MSVC

## 📁 核心架构概览
本项目架构遵循明确的职责分离模式（MVC/MVVM理念融合）：
1. **MeetingController (会议控制器)**：充当业务逻辑中枢，协调各模块运作并向 QML 输出响应接口。
2. **LiveKitManager (LiveKit 管理器)**：封装网络与房间逻辑，处理 Token 身份认证和由服务器下发的各类房间事件回调。
3. **MediaCapture (媒体采集器)**：直接管理系统的硬件设备捕获事件，以及承担 Qt Video/Audio 帧至 LiveKit 源数据的转换工作。
4. **ParticipantModel & ChatModel (数据模型)**：为 QML 的视图层（如 ListView/GridView）提供标准化、可绑定的状态数据源。

更详细的架构机制与数据流向设计，请参阅 [`summary.md`](./summary.md) 。

## 🚀 构建与运行指南 (Windows)
项目提供了便捷的自动构建脚本。您可以直接在项目根目录下的命令行或者终端中执行以下命令进行一键构建和运行：

1. **执行构建**：
   ```cmd
   .\build.cmd
   ```
2. **运行程序**：
   ```cmd
   .\build\release\MeetingApp.exe
   ```

*(该脚本将自动帮您初始化 MSVC、CMake、Ninja 及 Qt 的依赖环境并完成编译。如需了解自动脚本背后的构建具体原理或进行纯手工编译，请查看 [CMake 手动构建指南](./cmake_manual_build_guide.md))*

## 📄 更多技术文档
项目中包含了不同维度的开发手册：
- [架构概览及交互调用关系图](./summary.md)
- [视频流输入输出工作原理深究](./video_module_explanation.md)
- [Git PR 工作流团队协作指南](./github_pr_workflow.md)
- [扩展功能集成文档](./expend.md)
- [CLI 终端开发命令参考](./CLI_REFERENCE.md)

---
*Powered by Qt & LiveKit.*
