# 会议系统 AI 模块技术文档

> 本文档详细讲解项目中所有 AI 模块的调用原理、数据流和实现逻辑。

---

## 目录

1. [整体架构](#1-整体架构)
2. [AI 对话模块（智能问答）](#2-ai-对话模块智能问答)
3. [语音转文字模块（离线 ASR）](#3-语音转文字模块离线-asr)
4. [会议摘要模块](#4-会议摘要模块)
5. [会议纪要生成模块](#5-会议纪要生成模块)
6. [技术栈与 API 参考](#6-技术栈与-api-参考)

---

## 1. 整体架构

### 1.1 系统分层

```
┌────────────────────────────────────────────────────────────┐
│                      QML UI 层                              │
│   AIPanel.qml（对话/转录/纪要 三个 Tab 页面）                │
└───────────────┬──────────────────────────────┬──────────────┘
                │ Q_PROPERTY / Slots           │ Signals
┌───────────────▼──────────────────────────────▼──────────────┐
│                   C++ 中间层                                 │
│   AIAssistant 类                                            │
│   · 本地录音管理（PCM 缓冲、降采样、WAV 组装）                │
│   · HTTP 请求封装（QNetworkAccessManager）                   │
│   · 转录历史管理（TranscriptEntry 列表）                     │
└───────────────┬─────────────────────────────────────────────┘
                │ HTTP POST (JSON)
┌───────────────▼─────────────────────────────────────────────┐
│                   Node.js 服务端                             │
│   server.js (Express, port 3000)                            │
│   · /api/ai/chat        → 通义千问对话                       │
│   · /api/ai/transcribe  → DashScope Paraformer-v2 离线 ASR  │
│   · /api/ai/summarize   → 通义千问摘要生成                   │
│   · /api/ai/meeting-minutes → 通义千问会议纪要生成           │
│   · /api/ai/clear       → 清除对话历史                       │
└───────────────┬─────────────────────────────────────────────┘
                │ HTTPS
┌───────────────▼─────────────────────────────────────────────┐
│              阿里云 DashScope API                            │
│   · dashscope.aliyuncs.com/compatible-mode/v1/...           │
│     (通义千问 qwen-plus 大模型)                              │
│   · dashscope.aliyuncs.com/api/v1/services/audio/asr/...    │
│     (Paraformer-v2 语音识别)                                 │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 涉及的关键文件

| 文件 | 层级 | 职责 |
|------|------|------|
| `aliyun/server.js` | 服务端 | AI API 网关，调用 DashScope |
| `src/aiassistant.h` | C++ 头文件 | AI 助手类声明 |
| `src/aiassistant.cpp` | C++ 实现 | 录音管理 + HTTP 请求 |
| `src/main.cpp` | C++ 入口 | 信号/槽连接 |
| `resources/qml/components/AIPanel.qml` | QML UI | AI 面板界面 |
| `src/mediacapture.cpp` | C++ 实现 | 麦克风采集，提供 PCM 数据 |

### 1.3 公共基础：DashScope API Key

所有 AI 服务共用一个 DashScope API Key，仅存储在服务端：

```javascript
// server.js
const DASHSCOPE_API_KEY = process.env.DASHSCOPE_API_KEY || 'sk-xxxx';
```

> 客户端不存储 API Key，所有请求通过自有服务端中转，保证安全性。

---

## 2. AI 对话模块（智能问答）

### 2.1 功能描述

用户可以在会议中向 AI 助手"小会"提问，获取即时回答。支持多轮对话，AI 能记住上下文。

### 2.2 调用流程

```
用户在 AI 面板输入问题
       │
       ▼
QML: aiAssistant.sendMessage(message)
       │
       ▼
C++ AIAssistant::sendMessage()
  ├── 构建 JSON: { roomName, username, message }
  └── HTTP POST → 服务端 /api/ai/chat
       │
       ▼
服务端 /api/ai/chat 处理：
  ├── 1. getOrCreateSession(roomName)  // 获取该房间的对话历史
  ├── 2. 将用户消息追加到 session.history
  ├── 3. callAI(history, systemPrompt) // 调用通义千问
  ├── 4. 将 AI 回复追加到 session.history
  └── 5. 返回 { success: true, reply: "..." }
       │
       ▼
C++ 收到响应 → emit aiReplyReceived(reply)
       │
       ▼
QML Connections → onAiReplyReceived → 添加到聊天列表显示
```

### 2.3 核心函数：`callAI()`

```javascript
// server.js 第 69-123 行
async function callAI(messages, systemPrompt) {
    // 1. 如果有 systemPrompt，插入到消息列表最前面
    const allMessages = [];
    if (systemPrompt) {
        allMessages.push({ role: 'system', content: systemPrompt });
    }
    allMessages.push(...messages);

    // 2. 构建请求体 (OpenAI 兼容格式)
    const postData = JSON.stringify({
        model: 'qwen-plus',           // 通义千问模型
        messages: allMessages,         // 完整对话历史
        temperature: 0.7,              // 创造性程度
        max_tokens: 4096               // 最大回复长度
    });

    // 3. HTTPS POST 到 DashScope API
    // URL: https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions
    // Header: Authorization: Bearer <API_KEY>
    // ...
}
```

**关键点**：
- 使用 DashScope 的 **OpenAI 兼容模式**，API 路径为 `/compatible-mode/v1/chat/completions`
- 模型选择 `qwen-plus`（均衡的速度/质量平衡）
- 支持多轮对话：通过 `session.history` 维护完整消息链

### 2.4 会话管理

```javascript
// 每个房间独立维护一个会话
const roomAISessions = new Map();  // Map<roomName, SessionInfo>

// SessionInfo 结构：
{
    history: [         // 对话历史数组
        { role: 'user', content: '[张三]: 今天讨论什么？' },
        { role: 'assistant', content: '根据议程，今天主要讨论...' },
    ],
    createdAt: Date.now(),
    messageCount: 0
}
```

**防膨胀机制**：
- 对话历史超过 80 条时，自动截断到最近 60 条
- 超过 4 小时未活跃的会话自动清理

### 2.5 系统提示词

```javascript
const AI_SYSTEM_PROMPT = `你是一个会议AI助手，名叫"小会"。
你的能力：
1. 回答各种知识性问题
2. 帮助总结会议讨论内容
3. 提取待办事项和负责人
4. 翻译内容
5. 建议讨论议程
...`;
```

提示词定义了 AI 的角色、能力边界和回复风格规范。

---

## 3. 语音转文字模块（离线 ASR）

### 3.1 功能描述

用户录制一段会议音频，停止后自动将音频提交到 DashScope Paraformer-v2 进行离线语音识别，返回完整的转录文字。

### 3.2 完整调用流程

```
┌─────────────────────────────────────────────────────────────┐
│                    客户端（C++ / QML）                       │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 用户点击 "开始录音"                                      │
│     └→ QML 调用 aiAssistant.startRecording()                │
│        └→ 初始化 m_audioRecordBuffer, 启动计时器              │
│                                                             │
│  2. 麦克风持续采集 PCM 数据 (48kHz, 16-bit, mono)            │
│     └→ MediaCapture::rawAudioCaptured 信号触发               │
│        └→ AIAssistant::feedAudioData() 槽函数                │
│           └→ 降采样 48kHz→16kHz（均值滤波）                   │
│              └→ 追加到 m_audioRecordBuffer                   │
│                                                             │
│  3. 用户点击 "停止并转录"                                    │
│     └→ QML 调用 aiAssistant.stopRecordingAndTranscribe()    │
│        ├→ 停止录音, 获取 m_audioRecordBuffer                 │
│        ├→ buildWavFile() 组装 WAV 文件 (44字节头 + PCM)      │
│        ├→ WAV 转 base64                                      │
│        └→ HTTP POST /api/ai/transcribe                       │
│           Body: { roomName, audioData(base64), username }    │
│                                                             │
└──────────────────────────┬──────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────┐
│                    服务端（Node.js）                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  4. 接收 base64 WAV → Buffer.from(base64) → 写入临时文件     │
│     文件名: asr_<timestamp>_<uuid>.wav                       │
│     路径: os.tmpdir()/meeting_audio_temp/                    │
│                                                             │
│  5. 构建公网 URL                                             │
│     Express 静态目录: app.use('/audio-temp', express.static) │
│     URL: http://8.162.3.195:3000/audio-temp/asr_xxx.wav     │
│                                                             │
│  6. submitASRTask(fileUrl)                                   │
│     ├→ POST https://dashscope.aliyuncs.com/api/v1/          │
│     │       services/audio/asr/transcription                 │
│     ├→ Header: X-DashScope-Async: enable                    │
│     ├→ Body: { model: "paraformer-v2",                      │
│     │          input: { file_urls: [url] },                  │
│     │          parameters: { language_hints: ["zh","en"],    │
│     │                        disfluency_removal_enabled } }  │
│     └→ 返回 task_id                                          │
│                                                             │
│  7. waitForASRComplete(taskId)                               │
│     └→ 每 2 秒轮询:                                          │
│        GET https://dashscope.aliyuncs.com/api/v1/tasks/{id} │
│        直到 task_status === "SUCCEEDED" 或超时(5分钟)         │
│                                                             │
│  8. extractTranscriptText(results)                           │
│     ├→ 从 results[].transcription_url 下载转录 JSON          │
│     ├→ 解析 transcripts[].text 字段                          │
│     └→ 拼接后返回完整文本                                     │
│                                                             │
│  9. 返回 { success: true, transcript: "..." }                │
│                                                             │
└──────────────────────────┬──────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────┐
│                    客户端收到响应                             │
├─────────────────────────────────────────────────────────────┤
│  10. 解析 JSON → 保存到 m_transcripts 列表                   │
│      └→ emit transcriptCountChanged()                        │
│      └→ emit newTranscriptReceived(text)                     │
│      └→ emit transcriptionCompleted(text)                    │
│                                                             │
│  11. QML 更新 UI                                             │
│      └→ Connections.onNewTranscriptReceived                  │
│         └→ transcriptModel.append({speaker, time, text})     │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 3.3 音频降采样原理

麦克风采集 PCM 数据为 48kHz，DashScope ASR 要求 16kHz。降采样比 = 48000/16000 = 3。

```cpp
// aiassistant.cpp::feedAudioData()
// 对每 ratio(=3) 个输入样本取平均值 → 输出 1 个样本
// 这是简单的 "平均值低通滤波"，避免混叠噪声

for (int i = 0; i < outFrames; ++i) {
    int64_t sum = 0;
    for (int j = 0; j < ratio; ++j) {
        sum += src[i * ratio + j];   // 累加 3 个样本
    }
    dst[i] = sum / ratio;            // 取平均值
}
```

> 相比直接"每 3 个取 1 个"（点采样），均值滤波能有效抑制高频混叠噪声。

### 3.4 WAV 文件组装

```cpp
// aiassistant.cpp::buildWavFile()
// 标准 WAV 文件 = 44 字节文件头 + 原始 PCM 数据

RIFF 头 (12 字节):
  "RIFF" | chunkSize(4B) | "WAVE"

fmt 子块 (24 字节):
  "fmt " | 16(4B) | PCM=1(2B) | channels=1(2B)
  | sampleRate=16000(4B) | byteRate=32000(4B)
  | blockAlign=2(2B) | bitsPerSample=16(2B)

data 子块 (8 + N 字节):
  "data" | dataSize(4B) | [PCM 数据...]
```

### 3.5 DashScope Paraformer-v2 API 细节

**提交任务**：

| 字段 | 值 | 说明 |
|------|-----|------|
| URL | `POST /api/v1/services/audio/asr/transcription` | 固定端点 |
| Header | `X-DashScope-Async: enable` | **必须**，异步任务模式 |
| model | `paraformer-v2` | 离线 ASR 模型 |
| file_urls | `["http://...wav"]` | 公网可访问的音频 URL |
| language_hints | `["zh", "en"]` | 支持中英文混合识别 |
| disfluency_removal_enabled | `true` | 自动过滤"嗯""啊"等语气词 |

**查询任务**：

| 字段 | 值 | 说明 |
|------|-----|------|
| URL | `GET /api/v1/tasks/{task_id}` | 通过 task_id 查询 |
| task_status | `PENDING`→`RUNNING`→`SUCCEEDED`/`FAILED` | 状态流转 |
| transcription_url | OSS 上的 JSON URL | 包含详细转录结果 |

**转录结果 JSON 结构**：

```json
{
  "transcripts": [{
    "channel_id": 0,
    "text": "大家好，今天我们讨论一下项目进度。",
    "sentences": [{
      "begin_time": 100,
      "end_time": 3820,
      "text": "大家好，今天我们讨论一下项目进度。"
    }]
  }]
}
```

---

## 4. 会议摘要模块

### 4.1 功能描述

将会议聊天记录发送给 AI，自动生成结构化的讨论摘要。

### 4.2 调用流程

```
QML: aiAssistant.summarize(chatHistory)
  │
  ▼
C++ AIAssistant::summarize()
  ├── 将 QVariantList 转为 QJsonArray
  └── HTTP POST /api/ai/summarize
      Body: { roomName, chatHistory: [{sender, content, time}] }
  │
  ▼
服务端 /api/ai/summarize：
  ├── 1. 将 chatHistory 拼接为文本：
  │      "[14:30:05] 张三: 我觉得方案A更好"
  │      "[14:30:12] 李四: 我同意，但需要考虑成本"
  │
  ├── 2. 构建提示词：
  │      "请根据以下会议聊天记录，生成一份结构化的会议纪要。
  │       包含：会议主要议题、关键讨论点、达成的结论、待办事项。
  │       聊天记录：
  │       [14:30:05] 张三: ..."
  │
  └── 3. callAI([{role:'user', content: prompt}])
         → 返回摘要文本
  │
  ▼
C++ emit summaryReceived(summary)
  │
  ▼
QML: Connections.onSummaryReceived → 显示在聊天区
```

> **注意**：摘要接口 `/api/ai/summarize` **不带对话历史**（每次独立调用），这与 `/api/ai/chat` 的多轮对话模式不同。

---

## 5. 会议纪要生成模块

### 5.1 功能描述

综合**语音转录文本**和**聊天记录**两个数据源，生成专业的会议纪要。这是 AI 模块中最复杂的功能。

### 5.2 调用流程

```
QML: 用户点击 "生成会议纪要"
  │
  ▼
QML: aiAssistant.generateMeetingMinutes(chatHistory)
  │
  ▼
C++ AIAssistant::generateMeetingMinutes()
  ├── 从 m_transcripts 构建转录 JSON 数组
  ├── 从 chatHistory 构建聊天 JSON 数组
  └── HTTP POST /api/ai/meeting-minutes
      Body: {
          roomName,
          transcripts: [{time, speaker, text}, ...],
          chatHistory: [{sender, content, time}, ...]
      }
  │
  ▼
服务端 /api/ai/meeting-minutes：
  ├── 1. 构建提示词，包含两部分数据：
  │
  │   "请根据以下会议内容，生成一份专业的会议纪要。
  │    要求包含：
  │    1) 会议概要
  │    2) 主要议题讨论
  │    3) 关键决议
  │    4) 待办事项(标明负责人和截止时间)
  │    5) 下次会议安排
  │
  │    === 会议语音转录 ===
  │    [14:30:05] 张三: 今天主要讨论Q2的进度...
  │    [14:31:20] 李四: 目前开发进度已完成80%...
  │
  │    === 会议聊天记录 ===
  │    [14:30:10] 张三: 大家看一下共享屏幕
  │    [14:32:00] 王五: 我觉得时间节点需要调整"
  │
  └── 2. callAI([{role:'user', content: prompt}])
         → 返回会议纪要
  │
  ▼
C++ emit meetingMinutesReceived(minutes)
  │
  ▼
QML: Connections.onMeetingMinutesReceived
     → minutesContent = minutes (显示在纪要 Tab)
```

### 5.3 数据来源对比

| 数据源 | 来源 | 内容特点 |
|--------|------|---------|
| 语音转录 (`transcripts`) | 麦克风录音 → Paraformer-v2 ASR | 口语化，包含讨论细节 |
| 聊天记录 (`chatHistory`) | 会议中的文字聊天 | 简洁，包含链接/代码等 |

两个数据源互补：语音转录捕捉口头讨论，聊天记录捕捉文字交流。AI 综合两者生成完整纪要。

---

## 6. 技术栈与 API 参考

### 6.1 使用的 AI 模型

| 模型 | 提供商 | 用途 | 特点 |
|------|--------|------|------|
| `qwen-plus` | 阿里云通义千问 | 对话/摘要/纪要 | 均衡性价比，支持中英文 |
| `paraformer-v2` | 阿里云 DashScope | 语音转文字 | 专业 ASR，支持中英日韩 |

### 6.2 API 端点汇总

| 端点 | 方法 | 功能 | 超时 |
|------|------|------|------|
| `/api/ai/chat` | POST | AI 对话 | 60s |
| `/api/ai/summarize` | POST | 摘要生成 | 60s |
| `/api/ai/meeting-minutes` | POST | 纪要生成 | 60s |
| `/api/ai/transcribe` | POST | 语音转文字 | 6min |
| `/api/ai/clear` | POST | 清除对话历史 | - |

### 6.3 DashScope API 端点

| 端点 | 说明 |
|------|------|
| `https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions` | 通义千问对话（OpenAI 兼容） |
| `https://dashscope.aliyuncs.com/api/v1/services/audio/asr/transcription` | Paraformer 离线 ASR 提交 |
| `https://dashscope.aliyuncs.com/api/v1/tasks/{task_id}` | 异步任务状态查询 |

### 6.4 信号/槽连接关系

```
MediaCapture                AIAssistant               AIPanel.qml
═══════════                 ═══════════               ═══════════
rawAudioCaptured ──────────→ feedAudioData
                             │
                             ├→ aiReplyReceived ─────→ onAiReplyReceived
                             ├→ aiError ─────────────→ onAiError
                             ├→ summaryReceived ─────→ onSummaryReceived
                             ├→ meetingMinutesReceived→ onMeetingMinutesReceived
                             ├→ newTranscriptReceived → onNewTranscriptReceived
                             ├→ transcriptionCompleted→ onTranscriptionCompleted
                             └→ transcriptionFailed ─→ onTranscriptionFailed

LiveKitManager
═══════════
connected ─────────────────→ setServerUrl / setRoomName / setUserName
disconnected ──────────────→ setRoomName("")
```

### 6.5 客户端属性（QML 可绑定）

| 属性 | 类型 | 说明 |
|------|------|------|
| `isBusy` | bool | AI 请求进行中 |
| `isRecordingAudio` | bool | 正在录音 |
| `isTranscribing` | bool | 正在等待服务端转录结果 |
| `transcriptCount` | int | 已确认的转录段数 |
| `recordingDuration` | string | 录音时长 "MM:SS" |
| `lastReply` | string | AI 最近一次回复 |
| `lastError` | string | 最近一次错误信息 |
| `serverUrl` | string | 服务端地址 |
| `roomName` | string | 当前房间名 |
| `userName` | string | 当前用户名 |
