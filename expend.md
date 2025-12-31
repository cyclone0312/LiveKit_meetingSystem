# 视频会议系统 - 功能扩展规划

## 📊 当前完成度

### ✅ 已实现功能

| 功能          | 状态        | 说明                             |
| ------------- | ----------- | -------------------------------- |
| 本地视频采集  | ✅ 完成     | 摄像头采集、预览、发布到 LiveKit |
| 本地音频采集  | ✅ 基础完成 | 麦克风采集框架已搭建             |
| LiveKit 连接  | ✅ 完成     | Token 请求、房间连接、断开重连   |
| 会议创建/加入 | ✅ 完成     | 生成会议号、加入房间             |
| 基础 UI 框架  | ✅ 完成     | 视频网格、控制栏、顶部栏         |
| 模拟参会者    | ✅ 完成     | 用于界面开发测试                 |

### ⏳ 待实现功能

下面按优先级分为三个阶段

---

## 🎯 第一阶段：核心通信功能（优先级：高）

> **目标**：实现真正的多人视频会议，能看到对方、听到对方

### 1.1 远程视频接收与显示

**重要性**：⭐⭐⭐⭐⭐ 核心功能  
**预计工时**：2-3 天

**当前状态**：

- LiveKitRoomDelegate 已能接收 `onTrackSubscribed` 事件
- 但没有将远程视频流渲染到界面

**设计方案**：

```
┌─────────────────────────────────────────────────────────────────┐
│ 1. 创建 RemoteVideoRenderer 类                                  │
│    - 接收 livekit::RemoteVideoTrack                             │
│    - 创建 livekit::VideoStream 从轨道读取帧                      │
│    - 将 LKVideoFrame 转换为 QVideoFrame                          │
│    - 发送到 QVideoSink 供 QML VideoOutput 显示                   │
│                                                                  │
│ 2. 修改 ParticipantModel                                        │
│    - 添加 videoSink 属性，每个参会者一个                          │
│    - 远程参会者加入时创建 RemoteVideoRenderer                     │
│                                                                  │
│ 3. 修改 VideoItem.qml                                           │
│    - 根据 isLocal 选择使用 mediaCapture 还是远程 videoSink       │
└─────────────────────────────────────────────────────────────────┘
```

**关键代码位置**：

- 新建 `src/remotevideorenderer.h/cpp`
- 修改 `src/participantmodel.h/cpp`
- 修改 `src/livekitmanager.cpp` 的 `onTrackSubscribed`

---

### 1.2 远程音频接收与播放

**重要性**：⭐⭐⭐⭐⭐ 核心功能  
**预计工时**：1-2 天

**设计方案**：

```
┌─────────────────────────────────────────────────────────────────┐
│ 1. 创建 RemoteAudioPlayer 类                                    │
│    - 接收 livekit::RemoteAudioTrack                             │
│    - 创建 livekit::AudioStream 从轨道读取音频帧                   │
│    - 使用 QAudioSink 播放音频                                    │
│                                                                  │
│ 2. 音频管理                                                      │
│    - 多个远程参会者的音频混合                                     │
│    - 或每个参会者独立 AudioSink                                   │
└─────────────────────────────────────────────────────────────────┘
```

---

### 1.3 真实参会者列表同步

**重要性**：⭐⭐⭐⭐⭐ 核心功能  
**预计工时**：1 天

**当前状态**：

- `onParticipantConnected/Disconnected` 已能接收事件
- 但没有同步到 ParticipantModel

**设计方案**：

```
┌─────────────────────────────────────────────────────────────────┐
│ 1. 连接 LiveKitManager 信号到 ParticipantModel                  │
│    - participantJoined → addParticipant()                       │
│    - participantLeft → removeParticipant()                      │
│                                                                  │
│ 2. 连接后获取已有参会者                                          │
│    - 连接成功后遍历 room->remoteParticipants()                   │
│    - 添加到 ParticipantModel                                     │
│                                                                  │
│ 3. 移除模拟数据                                                  │
│    - 删除 addDemoParticipants() 调用                             │
└─────────────────────────────────────────────────────────────────┘
```

---

### 1.4 音频发布完善

**重要性**：⭐⭐⭐⭐ 核心功能  
**预计工时**：0.5 天

**当前状态**：

- 音频采集框架已搭建
- `publishMicrophone()` 方法已存在但未完善

**设计方案**：

- 参考 `publishCamera()` 的实现
- 添加 `doPublishMicrophoneTrack()` 内部方法
- 实现 mute/unmute 控制

---

## 🎯 第二阶段：会议交互功能（优先级：中）

> **目标**：完善会议体验，支持聊天、举手等互动

### 2.1 实时聊天功能

**重要性**：⭐⭐⭐⭐  
**预计工时**：1-2 天

**当前状态**：

- ChatModel 数据模型已完成
- ChatPanel.qml UI 已完成
- LiveKitManager 有 `sendData()` 和 `onUserPacketReceived()`

**设计方案**：

```
┌─────────────────────────────────────────────────────────────────┐
│ 1. 发送消息                                                      │
│    ChatPanel 输入 → ChatModel.sendMessage()                      │
│                   → LiveKitManager.sendChatMessage()             │
│                   → room.localParticipant().publishData()        │
│                                                                  │
│ 2. 接收消息                                                      │
│    onUserPacketReceived() → 解析 JSON                            │
│                           → 发出 chatMessageReceived 信号        │
│                           → ChatModel.addMessage()               │
│                                                                  │
│ 3. 消息格式 (JSON)                                               │
│    {                                                             │
│      "type": "chat",                                             │
│      "senderId": "xxx",                                          │
│      "senderName": "张三",                                       │
│      "content": "Hello",                                         │
│      "timestamp": 1234567890                                     │
│    }                                                             │
└─────────────────────────────────────────────────────────────────┘
```

---

### 2.2 举手功能

**重要性**：⭐⭐⭐  
**预计工时**：0.5 天

**设计方案**：

```
┌─────────────────────────────────────────────────────────────────┐
│ 1. 通过 Data Channel 广播举手状态                                │
│    { "type": "handRaise", "raised": true }                      │
│                                                                  │
│ 2. 收到后更新 ParticipantModel                                  │
│    setParticipantHandRaised(identity, raised)                   │
│                                                                  │
│ 3. UI 显示举手图标                                               │
│    VideoItem 中根据 isHandRaised 显示 🖐️ 图标                    │
└─────────────────────────────────────────────────────────────────┘
```

---

### 2.3 麦克风/摄像头状态同步

**重要性**：⭐⭐⭐⭐  
**预计工时**：1 天

**设计方案**：

```
┌─────────────────────────────────────────────────────────────────┐
│ 1. 监听 onTrackMuted/Unmuted 事件                               │
│                                                                  │
│ 2. 根据轨道类型更新 ParticipantModel                            │
│    - 视频轨道 muted → updateParticipant(id, mic, camera=false)  │
│    - 音频轨道 muted → updateParticipant(id, mic=false, camera)  │
│                                                                  │
│ 3. UI 显示状态图标                                               │
│    VideoItem 中显示 🎤❌ 或 📷❌ 图标                             │
└─────────────────────────────────────────────────────────────────┘
```

---

### 2.4 设备选择功能

**重要性**：⭐⭐⭐  
**预计工时**：1 天

**当前状态**：

- MediaCapture 已有 `availableCameras/Microphones` 属性
- 已有 `setCurrentCameraIndex/MicrophoneIndex` 方法

**设计方案**：

```
┌─────────────────────────────────────────────────────────────────┐
│ 1. 创建 SettingsDialog.qml                                      │
│    - 摄像头下拉列表                                              │
│    - 麦克风下拉列表                                              │
│    - 扬声器下拉列表                                              │
│    - 预览窗口                                                    │
│                                                                  │
│ 2. 绑定到 MediaCapture 属性                                     │
│    ComboBox.model: mediaCapture.availableCameras                │
│    ComboBox.currentIndex: mediaCapture.currentCameraIndex       │
└─────────────────────────────────────────────────────────────────┘
```

---

## 🎯 第三阶段：高级功能（优先级：低）

> **目标**：提升专业性，增加高级特性

### 3.1 屏幕共享

**重要性**：⭐⭐⭐  
**预计工时**：3-5 天

**设计方案**：

```
┌─────────────────────────────────────────────────────────────────┐
│ 1. 使用 Qt 的屏幕采集 API                                       │
│    - QScreen::grabWindow() 或                                   │
│    - Windows: DXGI Desktop Duplication API                      │
│                                                                  │
│ 2. 创建 ScreenCapture 类                                        │
│    - 枚举可共享的窗口/屏幕                                       │
│    - 采集屏幕帧                                                  │
│    - 发送到 LiveKit VideoSource                                 │
│                                                                  │
│ 3. 发布为独立轨道                                                │
│    - 轨道名 "screen" 区别于 "camera"                            │
│    - 接收方判断轨道类型显示到不同位置                             │
└─────────────────────────────────────────────────────────────────┘
```

---

### 3.2 会议录制

**重要性**：⭐⭐  
**预计工时**：3-5 天

**设计方案**：

```
┌─────────────────────────────────────────────────────────────────┐
│ 方案A：本地录制                                                  │
│    - 使用 FFmpeg 将视频帧和音频帧编码为 MP4                       │
│    - 需要处理多路视频的合成布局                                   │
│                                                                  │
│ 方案B：服务端录制（推荐）                                        │
│    - 使用 LiveKit 的 Egress 功能                                 │
│    - 服务端自动录制并存储                                        │
│    - 客户端只需调用 API 开始/停止                                │
└─────────────────────────────────────────────────────────────────┘
```

---

### 3.3 虚拟背景

**重要性**：⭐⭐  
**预计工时**：5-7 天

**设计方案**：

```
┌─────────────────────────────────────────────────────────────────┐
│ 1. 人像分割                                                      │
│    - 使用 OpenCV + 深度学习模型（如 MediaPipe）                   │
│    - 或使用 Windows Vision API                                   │
│                                                                  │
│ 2. 背景替换                                                      │
│    - 将背景区域替换为图片/模糊效果                                │
│    - 在 VideoFrameHandler 中处理每一帧                           │
│                                                                  │
│ 3. 性能优化                                                      │
│    - GPU 加速                                                    │
│    - 降低处理分辨率                                              │
└─────────────────────────────────────────────────────────────────┘
```

---

### 3.4 噪声抑制 / 回声消除

**重要性**：⭐⭐⭐  
**预计工时**：2-3 天

**设计方案**：

```
┌─────────────────────────────────────────────────────────────────┐
│ 方案A：使用 WebRTC 内置的音频处理                                │
│    - LiveKit SDK 可能已内置                                      │
│                                                                  │
│ 方案B：集成 RNNoise                                              │
│    - 开源的噪声抑制库                                            │
│    - 在 AudioFrameHandler 中处理                                 │
│                                                                  │
│ 方案C：使用 Windows 音频增强 API                                 │
│    - Windows.Media.Audio.AudioEffectsManager                    │
└─────────────────────────────────────────────────────────────────┘
```

---

### 3.5 会议安全功能

**重要性**：⭐⭐  
**预计工时**：1-2 天

**设计方案**：

```
┌─────────────────────────────────────────────────────────────────┐
│ 1. 会议密码                                                      │
│    - Token Server 验证密码                                       │
│    - 密码错误不发放 Token                                        │
│                                                                  │
│ 2. 等候室                                                        │
│    - 新参会者先进入等候状态                                       │
│    - 主持人批准后才能加入                                        │
│                                                                  │
│ 3. 锁定会议                                                      │
│    - 主持人锁定后禁止新人加入                                     │
└─────────────────────────────────────────────────────────────────┘
```

---

## 🎯 第四阶段：用户系统与数据库（优先级：中高）

> **目标**：实现完整的用户账号体系，支持注册、登录、个人信息管理

### 4.1 后端服务搭建

**重要性**：⭐⭐⭐⭐⭐ 基础设施  
**预计工时**：2-3 天

**技术选型**：

```
┌─────────────────────────────────────────────────────────────────┐
│ 推荐方案：Node.js + Express + MySQL/PostgreSQL                   │
│                                                                  │
│ 为什么选这个：                                                    │
│  - 现有 Token Server 已经是 Node.js，可以直接扩展                 │
│  - Express 简单易用，适合快速开发                                 │
│  - MySQL 成熟稳定，学习资料丰富                                   │
│                                                                  │
│ 备选方案：                                                       │
│  - Python + FastAPI + PostgreSQL                                │
│  - Go + Gin + MySQL                                             │
└─────────────────────────────────────────────────────────────────┘
```

**服务架构**：

```
┌─────────────────────────────────────────────────────────────────┐
│                         后端服务架构                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│   Qt 客户端                                                      │
│       │                                                          │
│       ▼                                                          │
│   ┌──────────────────────────────────────────┐                  │
│   │           API Server (Node.js)           │                  │
│   │  ┌────────────┬────────────┬──────────┐  │                  │
│   │  │ /api/auth  │ /api/user  │/api/meet │  │                  │
│   │  │  登录注册   │  用户信息   │ 会议管理  │  │                  │
│   │  └────────────┴────────────┴──────────┘  │                  │
│   │               │                          │                  │
│   │               ▼                          │                  │
│   │      ┌──────────────────┐                │                  │
│   │      │   MySQL 数据库    │                │                  │
│   │      └──────────────────┘                │                  │
│   └──────────────────────────────────────────┘                  │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

### 4.2 数据库设计

**重要性**：⭐⭐⭐⭐⭐ 基础设施  
**预计工时**：1 天

**数据库表结构**：

```sql
-- =====================================================
-- 用户表
-- =====================================================
CREATE TABLE users (
    id              INT PRIMARY KEY AUTO_INCREMENT,
    username        VARCHAR(50) UNIQUE NOT NULL,      -- 用户名
    email           VARCHAR(100) UNIQUE,              -- 邮箱（可选）
    phone           VARCHAR(20) UNIQUE,               -- 手机号（可选）
    password_hash   VARCHAR(255) NOT NULL,            -- 密码哈希（bcrypt）
    nickname        VARCHAR(50),                      -- 显示昵称
    avatar_url      VARCHAR(255),                     -- 头像URL
    created_at      TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at      TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    last_login_at   TIMESTAMP,                        -- 最后登录时间
    status          ENUM('active', 'disabled') DEFAULT 'active'
);

-- =====================================================
-- 会议记录表
-- =====================================================
CREATE TABLE meetings (
    id              INT PRIMARY KEY AUTO_INCREMENT,
    meeting_id      VARCHAR(20) UNIQUE NOT NULL,      -- 会议号
    title           VARCHAR(100),                     -- 会议标题
    host_user_id    INT NOT NULL,                     -- 主持人ID
    password        VARCHAR(50),                      -- 会议密码（可选）
    start_time      TIMESTAMP,                        -- 开始时间
    end_time        TIMESTAMP,                        -- 结束时间
    status          ENUM('scheduled', 'active', 'ended') DEFAULT 'scheduled',
    max_participants INT DEFAULT 100,                 -- 最大参会人数
    created_at      TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (host_user_id) REFERENCES users(id)
);

-- =====================================================
-- 参会记录表
-- =====================================================
CREATE TABLE meeting_participants (
    id              INT PRIMARY KEY AUTO_INCREMENT,
    meeting_id      INT NOT NULL,
    user_id         INT NOT NULL,
    join_time       TIMESTAMP,                        -- 加入时间
    leave_time      TIMESTAMP,                        -- 离开时间
    role            ENUM('host', 'co-host', 'participant') DEFAULT 'participant',
    FOREIGN KEY (meeting_id) REFERENCES meetings(id),
    FOREIGN KEY (user_id) REFERENCES users(id)
);

-- =====================================================
-- 会议聊天记录表（可选）
-- =====================================================
CREATE TABLE chat_messages (
    id              INT PRIMARY KEY AUTO_INCREMENT,
    meeting_id      INT NOT NULL,
    user_id         INT NOT NULL,
    content         TEXT NOT NULL,
    message_type    ENUM('text', 'file', 'system') DEFAULT 'text',
    created_at      TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (meeting_id) REFERENCES meetings(id),
    FOREIGN KEY (user_id) REFERENCES users(id)
);
```

---

### 4.3 用户注册功能

**重要性**：⭐⭐⭐⭐  
**预计工时**：1-2 天

**API 设计**：

```
POST /api/auth/register
Request:
{
    "username": "zhangsan",
    "password": "123456",
    "nickname": "张三",
    "email": "zhangsan@example.com"  // 可选
}

Response (成功):
{
    "success": true,
    "message": "注册成功",
    "data": {
        "userId": 1,
        "username": "zhangsan"
    }
}

Response (失败):
{
    "success": false,
    "message": "用户名已存在"
}
```

**客户端实现**：

```
┌─────────────────────────────────────────────────────────────────┐
│ 1. 创建 RegisterPage.qml                                        │
│    - 用户名输入框                                                │
│    - 密码输入框 + 确认密码                                       │
│    - 昵称输入框                                                  │
│    - 邮箱输入框（可选）                                          │
│    - 注册按钮                                                    │
│                                                                  │
│ 2. 创建 UserService 类（或扩展 MeetingController）               │
│    - register(username, password, nickname)                     │
│    - 发送 HTTP POST 请求                                        │
│    - 处理响应，发出信号                                          │
│                                                                  │
│ 3. 密码安全                                                      │
│    - 前端：密码强度检查（长度、复杂度）                           │
│    - 后端：bcrypt 哈希存储，永不存储明文                          │
└─────────────────────────────────────────────────────────────────┘
```

---

### 4.4 用户登录功能

**重要性**：⭐⭐⭐⭐⭐  
**预计工时**：1-2 天

**API 设计**：

```
POST /api/auth/login
Request:
{
    "username": "zhangsan",
    "password": "123456"
}

Response (成功):
{
    "success": true,
    "data": {
        "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
        "user": {
            "id": 1,
            "username": "zhangsan",
            "nickname": "张三",
            "avatarUrl": "https://..."
        }
    }
}
```

**Token 机制**：

```
┌─────────────────────────────────────────────────────────────────┐
│                       JWT Token 流程                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  1. 用户登录 → 服务器验证 → 生成 JWT Token                       │
│                                                                  │
│  2. 客户端存储 Token（QSettings 或内存）                         │
│                                                                  │
│  3. 后续请求携带 Token                                           │
│     Header: Authorization: Bearer <token>                       │
│                                                                  │
│  4. 获取 LiveKit Token 时验证用户 Token                          │
│     POST /api/livekit/token                                     │
│     Header: Authorization: Bearer <user_token>                  │
│     Body: { "roomName": "xxx" }                                 │
│                                                                  │
│  5. Token 过期处理                                               │
│     - 短期 Token (如 2小时) + Refresh Token (如 7天)            │
│     - 或简单方案：长期 Token (如 30天)                           │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**客户端实现**：

```cpp
// 修改 LoginPage.qml，调用真实登录接口
// 登录成功后保存 Token
QSettings settings;
settings.setValue("auth/token", token);
settings.setValue("auth/userId", userId);
```

---

### 4.5 会议历史记录

**重要性**：⭐⭐⭐  
**预计工时**：1 天

**功能设计**：

```
┌─────────────────────────────────────────────────────────────────┐
│ 1. 会议创建时记录到数据库                                        │
│    - 会议ID、标题、主持人、开始时间                               │
│                                                                  │
│ 2. 参会者加入/离开时记录                                         │
│    - meeting_participants 表                                    │
│                                                                  │
│ 3. 会议结束时更新状态                                            │
│    - 记录结束时间                                                │
│                                                                  │
│ 4. 客户端查看历史                                                │
│    - 主页显示"最近会议"列表                                      │
│    - 可以重新加入（如果会议还在进行）                             │
│    - 可以查看会议详情（参会者、时长等）                           │
└─────────────────────────────────────────────────────────────────┘
```

---

## 🎯 第五阶段：AI 智能功能（优先级：高 - 亮点功能）

> **目标**：集成 AI 能力，提升产品竞争力和演示效果

### 5.1 AI 会议纪要（方案 A - 推荐先做）

**重要性**：⭐⭐⭐⭐⭐ 亮点功能  
**预计工时**：2-3 天  
**技术难度**：⭐⭐ 中等

**功能描述**：

- 会议中点击"开始录音"，录制会议音频
- 会议结束后点击"生成纪要"
- AI 自动生成会议摘要、要点、待办事项

**系统架构**：

```
┌─────────────────────────────────────────────────────────────────┐
│                      AI 会议纪要架构                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│   Qt 客户端                                                      │
│   ┌────────────────────────────────────────┐                    │
│   │  [开始录音] ──► 录音中... ──► [结束录音] │                    │
│   │       │                          │      │                    │
│   │       ▼                          ▼      │                    │
│   │  QAudioSource              保存 WAV 文件 │                    │
│   │  采集麦克风音频             (本地临时文件) │                    │
│   │                                  │      │                    │
│   │                                  ▼      │                    │
│   │                          [生成会议纪要]  │                    │
│   └───────────────────────────────│────────┘                    │
│                                   │                              │
│                                   ▼ HTTP POST (multipart/form)  │
│   ┌────────────────────────────────────────┐                    │
│   │            后端 API Server              │                    │
│   │                                        │                    │
│   │  POST /api/ai/meeting-summary          │                    │
│   │    1. 接收音频文件                      │                    │
│   │    2. 调用 Whisper API 语音转文字       │                    │
│   │    3. 调用 Gemini API 生成摘要          │                    │
│   │    4. 返回结构化结果                    │                    │
│   └────────────────────────────────────────┘                    │
│                   │                                              │
│                   ▼                                              │
│   ┌────────────────────────────────────────┐                    │
│   │         Google AI Studio API           │                    │
│   │  - Gemini Pro (文本生成)               │                    │
│   │  - 或 Whisper (语音识别)               │                    │
│   └────────────────────────────────────────┘                    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**实现步骤**：

**Step 1: 客户端录音功能**

```cpp
// 新建 AudioRecorder 类
class AudioRecorder : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isRecording READ isRecording NOTIFY recordingChanged)

public:
    Q_INVOKABLE void startRecording();   // 开始录音
    Q_INVOKABLE void stopRecording();    // 停止录音
    Q_INVOKABLE QString getRecordingPath(); // 获取录音文件路径

private:
    QAudioSource* m_audioSource;
    QFile* m_outputFile;
    bool m_isRecording = false;
};
```

**Step 2: 后端 AI 接口**

```javascript
// Node.js 后端
const { GoogleGenerativeAI } = require("@google/generative-ai");

app.post(
  "/api/ai/meeting-summary",
  upload.single("audio"),
  async (req, res) => {
    // 1. 语音转文字（可用 Google Speech-to-Text 或 Whisper）
    const transcript = await speechToText(req.file.path);

    // 2. 调用 Gemini 生成摘要
    const genAI = new GoogleGenerativeAI(process.env.GEMINI_API_KEY);
    const model = genAI.getGenerativeModel({ model: "gemini-pro" });

    const prompt = `
        请根据以下会议记录生成会议纪要，包括：
        1. 会议摘要（2-3句话）
        2. 主要讨论点（列表）
        3. 决定事项（列表）
        4. 待办事项（列表，包括负责人如果提到）
        
        会议记录：
        ${transcript}
    `;

    const result = await model.generateContent(prompt);

    res.json({
      success: true,
      data: {
        transcript: transcript,
        summary: result.response.text(),
      },
    });
  }
);
```

**Step 3: 客户端显示结果**

```qml
// MeetingSummaryDialog.qml
Dialog {
    title: "AI 会议纪要"

    ScrollView {
        TextArea {
            text: summaryText
            readOnly: true
            wrapMode: TextArea.Wrap
        }
    }

    footer: DialogButtonBox {
        Button { text: "复制"; onClicked: copyToClipboard() }
        Button { text: "保存"; onClicked: saveToFile() }
        Button { text: "关闭"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
    }
}
```

---

### 5.2 AI 语音助手（方案 B - 最酷炫）

**重要性**：⭐⭐⭐⭐⭐ 核心亮点  
**预计工时**：3-5 天  
**技术难度**：⭐⭐⭐ 较高

**功能描述**：

- 会议中有一个 AI 机器人参会者
- 用户可以语音提问："你好 AI，请介绍一下 WebRTC"
- AI 会用语音回答，所有参会者都能听到

**为什么推荐**：

- LiveKit 官方提供 Python Agents 框架，支持极好
- 实现"实时语音对话"，技术含量高，演示效果震撼
- 体现对前沿技术的掌握

**系统架构**：

```
┌─────────────────────────────────────────────────────────────────┐
│                    AI 语音助手架构                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│                      LiveKit Server                              │
│                           │                                      │
│         ┌─────────────────┼─────────────────┐                   │
│         │                 │                 │                   │
│         ▼                 ▼                 ▼                   │
│   ┌──────────┐     ┌──────────┐     ┌──────────────┐           │
│   │ Qt 客户端 │     │ Qt 客户端 │     │ Python Agent │           │
│   │  (用户A)  │     │  (用户B)  │     │  (AI 机器人)  │           │
│   └──────────┘     └──────────┘     └──────────────┘           │
│                                            │                    │
│                                            │ 音频流              │
│                                            ▼                    │
│                                     ┌──────────────┐           │
│                                     │ 语音识别 STT │           │
│                                     │ (Deepgram/   │           │
│                                     │  Whisper)    │           │
│                                     └──────┬───────┘           │
│                                            │ 文本               │
│                                            ▼                    │
│                                     ┌──────────────┐           │
│                                     │   LLM 推理   │           │
│                                     │ (Gemini/     │           │
│                                     │  OpenAI)     │           │
│                                     └──────┬───────┘           │
│                                            │ 回复文本           │
│                                            ▼                    │
│                                     ┌──────────────┐           │
│                                     │ 语音合成 TTS │           │
│                                     │ (Google TTS/ │           │
│                                     │  ElevenLabs) │           │
│                                     └──────┬───────┘           │
│                                            │ 音频流              │
│                                            ▼                    │
│                                     发布到 LiveKit 房间          │
│                                     (所有人都能听到)             │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**实现步骤**：

**Step 1: 安装 LiveKit Agents**

```bash
pip install livekit-agents livekit-plugins-google livekit-plugins-silero
```

**Step 2: 创建 AI Agent 脚本**

```python
# ai_agent.py
from livekit.agents import AutoSubscribe, JobContext, WorkerOptions, cli
from livekit.agents.voice_assistant import VoiceAssistant
from livekit.plugins import google, silero

async def entrypoint(ctx: JobContext):
    # 连接到房间
    await ctx.connect(auto_subscribe=AutoSubscribe.AUDIO_ONLY)

    # 创建语音助手
    assistant = VoiceAssistant(
        vad=silero.VAD.load(),              # 语音活动检测
        stt=google.STT(),                    # 语音转文字
        llm=google.LLM(model="gemini-pro"),  # 大语言模型
        tts=google.TTS(),                    # 文字转语音
    )

    # 启动助手
    assistant.start(ctx.room)

    # 打招呼
    await assistant.say("你好，我是 AI 助手，有什么可以帮你的？")

if __name__ == "__main__":
    cli.run_app(WorkerOptions(entrypoint_fnc=entrypoint))
```

**Step 3: 运行 Agent**

```bash
# 设置环境变量
export LIVEKIT_URL=ws://your-server:7880
export LIVEKIT_API_KEY=your-api-key
export LIVEKIT_API_SECRET=your-api-secret
export GOOGLE_API_KEY=your-gemini-key

# 启动 Agent Worker
python ai_agent.py start
```

**Step 4: 触发 Agent 加入房间**

```javascript
// 后端 API：邀请 AI 加入会议
app.post("/api/ai/join-meeting", async (req, res) => {
  const { roomName } = req.body;

  // 调用 LiveKit Agent Dispatch API
  // 或者让 Agent 监听特定房间前缀自动加入

  res.json({ success: true, message: "AI 助手已加入会议" });
});
```

**Step 5: Qt 客户端添加"邀请 AI"按钮**

```qml
// ControlBar.qml 添加按钮
MeetingButton {
    icon: "🤖"
    text: "AI助手"
    onClicked: {
        meetingController.inviteAIAssistant()
    }
}
```

---

### 5.3 AI 实时字幕

**重要性**：⭐⭐⭐⭐  
**预计工时**：2-3 天

**功能描述**：

- 实时显示所有参会者的语音转文字
- 支持多语言翻译

**设计方案**：

```
┌─────────────────────────────────────────────────────────────────┐
│ 方案A：客户端本地识别                                            │
│    - 使用 Whisper.cpp 本地模型                                   │
│    - 优点：无网络延迟，隐私性好                                   │
│    - 缺点：需要较高 CPU/GPU 性能                                 │
│                                                                  │
│ 方案B：服务端识别                                                │
│    - 音频流发送到后端                                            │
│    - 后端调用 Google Speech-to-Text API                         │
│    - 通过 WebSocket 推送字幕                                     │
│    - 优点：识别准确率高                                          │
│    - 缺点：有延迟，需要消耗 API 额度                             │
│                                                                  │
│ 方案C：LiveKit Agent 方式                                       │
│    - Agent 订阅所有音频流                                        │
│    - 识别后通过 Data Channel 广播字幕                            │
│    - 优点：架构清晰，与语音助手复用                               │
└─────────────────────────────────────────────────────────────────┘
```

---

### 5.4 AI 功能 API Key 配置

**重要性**：⭐⭐⭐⭐⭐ 前置条件

**Google AI Studio 配置**：

```
┌─────────────────────────────────────────────────────────────────┐
│ 1. 访问 https://aistudio.google.com/                            │
│                                                                  │
│ 2. 创建 API Key                                                 │
│    - 点击 "Get API Key"                                         │
│    - 创建新项目或选择已有项目                                     │
│    - 复制生成的 API Key                                         │
│                                                                  │
│ 3. 配置环境变量                                                  │
│    - 后端：GOOGLE_API_KEY=AIza...                               │
│    - 或写入配置文件                                              │
│                                                                  │
│ 4. 免费额度                                                      │
│    - Gemini Pro: 60 QPM (每分钟请求数)                          │
│    - 足够演示和测试使用                                          │
└─────────────────────────────────────────────────────────────────┘
```

---

## 📅 推荐实现顺序（更新版）

```
                     ┌─────────────────────────────────────────┐
                     │           第一阶段：核心通信              │
                     │    (让多人会议真正能用)                   │
                     └─────────────────┬───────────────────────┘
                                       │
         ┌─────────────────────────────┼─────────────────────────────┐
         ▼                             ▼                             ▼
   ┌──────────┐                ┌──────────────┐               ┌──────────┐
   │ 1.3      │                │ 1.1          │               │ 1.2      │
   │参会者同步 │ ────────────▶  │ 远程视频显示  │ ────────────▶ │ 远程音频  │
   │ (1天)    │                │ (2-3天)      │               │ (1-2天)  │
   └──────────┘                └──────────────┘               └──────────┘
                                                                    │
                     ┌──────────────────────────────────────────────┘
                     ▼
              第一阶段完成 ✅
           （可进行多人会议）
                     │
    ┌────────────────┴────────────────┐
    ▼                                 ▼
┌─────────────────────┐    ┌─────────────────────┐
│  第四阶段：用户系统   │    │  第五阶段：AI 功能    │
│  (可并行开发)        │    │  (可并行开发)        │
└─────────┬───────────┘    └─────────┬───────────┘
          │                          │
    ┌─────┴─────┐              ┌─────┴─────┐
    ▼           ▼              ▼           ▼
┌────────┐ ┌────────┐    ┌──────────┐ ┌──────────┐
│4.1后端 │ │4.2数据库│    │5.1会议纪要│ │5.2AI助手 │
│搭建    │ │设计    │    │(推荐先做) │ │(最酷炫)  │
│(2-3天) │ │(1天)   │    │(2-3天)   │ │(3-5天)   │
└────────┘ └────────┘    └──────────┘ └──────────┘
    │           │              │           │
    ▼           ▼              │           │
┌────────┐ ┌────────┐          │           │
│4.3注册 │ │4.4登录 │          │           │
│(1-2天) │ │(1-2天) │          │           │
└────────┘ └────────┘          │           │
                               │           │
    ┌──────────────────────────┴───────────┘
    ▼
┌─────────────────────────────────────────┐
│          第二阶段：会议交互功能           │
│  聊天、状态同步、设备选择等               │
│           (3-5天)                        │
└─────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────┐
│          第三阶段：高级功能              │
│  屏幕共享、录制、虚拟背景等              │
│          (按需实现)                      │
└─────────────────────────────────────────┘
```

### 🎯 推荐优先级排序

| 优先级     | 功能             | 工时   | 原因                     |
| ---------- | ---------------- | ------ | ------------------------ |
| ⭐⭐⭐⭐⭐ | 1.1-1.4 核心通信 | 5-7 天 | 基础功能，必须完成       |
| ⭐⭐⭐⭐⭐ | 5.1 AI 会议纪要  | 2-3 天 | **亮点功能**，演示效果好 |
| ⭐⭐⭐⭐⭐ | 5.2 AI 语音助手  | 3-5 天 | **核心亮点**，技术含量高 |
| ⭐⭐⭐⭐   | 4.1-4.4 用户系统 | 5-7 天 | 完整产品必需             |
| ⭐⭐⭐     | 2.x 会议交互     | 3-5 天 | 提升体验                 |
| ⭐⭐       | 3.x 高级功能     | 按需   | 锦上添花                 |

### 💡 开发建议

**如果时间有限（如毕设答辩前）**：

1. 先完成核心通信（1.1-1.4）
2. 直接做 AI 会议纪要（5.1）—— 2-3 天搞定，演示效果很好
3. 用户系统可以简化（保持现有的本地登录）

**如果时间充裕**：

1. 核心通信 → AI 语音助手 → 用户系统 → 会议交互 → 高级功能

---

## 🚀 快速开始建议

### 下一步应该做什么？

**方案 A：稳扎稳打**

```
1. 先完成 1.3 参会者同步 (1天)
2. 然后 1.1 远程视频显示 (2-3天)
3. 接着 5.1 AI 会议纪要 (2-3天)
```

**方案 B：快速出亮点**

```
1. 先完成 5.1 AI 会议纪要 (2-3天)
   - 不依赖远程视频功能
   - 可以独立演示
   - 立刻看到 AI 效果

2. 再做核心通信 (5-7天)
```

### 5.1 AI 会议纪要快速实现指南

**这是最推荐先做的功能，因为：**

- 不依赖多人通信，单机就能测试
- 实现简单（录音 + 调 API）
- 演示效果好，商务感强

**最小可行版本（1 天）**：

```cpp
// 1. 在 MeetingController 添加录音控制
Q_INVOKABLE void startRecording();
Q_INVOKABLE void stopRecording();
Q_INVOKABLE void generateSummary();

// 2. 录音结束后发送到后端
// POST /api/ai/summary
// Body: { audio_base64: "..." }

// 3. 后端调用 Gemini API
// 返回摘要文本

// 4. 弹窗显示结果
```

---

## 📚 参考资源

### LiveKit 相关

- [LiveKit C++ SDK 文档](https://docs.livekit.io/client-sdk-cpp/)
- [LiveKit Python Agents](https://docs.livekit.io/agents/)
- [LiveKit Agents Examples](https://github.com/livekit/agents/tree/main/examples)

### AI 相关

- [Google AI Studio](https://aistudio.google.com/)
- [Gemini API 文档](https://ai.google.dev/docs)
- [LiveKit + OpenAI 实时语音](https://docs.livekit.io/agents/openai/)

### Qt 相关

- [Qt Multimedia 文档](https://doc.qt.io/qt-6/qtmultimedia-index.html)
- [QAudioSource 录音](https://doc.qt.io/qt-6/qaudiosource.html)
- [Qt Network HTTP 请求](https://doc.qt.io/qt-6/qnetworkaccessmanager.html)

### 数据库相关

- [Node.js + MySQL](https://www.npmjs.com/package/mysql2)
- [Express.js 文档](https://expressjs.com/)
- [JWT 认证](https://jwt.io/introduction)
- [bcrypt 密码哈希](https://www.npmjs.com/package/bcrypt)
