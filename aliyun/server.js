const express = require('express');
const { AccessToken, RoomServiceClient } = require('livekit-server-sdk');
const cors = require('cors');
const mysql = require('mysql2/promise');
const bcrypt = require('bcrypt');
const https = require('https');
const http = require('http');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const os = require('os');

const app = express();
const port = 3000;

app.use(cors());
app.use(express.json({ limit: '200mb' }));
app.use(express.urlencoded({ limit: '200mb', extended: true }));

// ==================== 临时音频文件目录 ====================
const AUDIO_TEMP_DIR = path.join(os.tmpdir(), 'meeting_audio_temp');
if (!fs.existsSync(AUDIO_TEMP_DIR)) {
    fs.mkdirSync(AUDIO_TEMP_DIR, { recursive: true });
}
// 提供临时音频文件的静态访问（DashScope ASR 需要公网 URL）
app.use('/audio-temp', express.static(AUDIO_TEMP_DIR));
console.log(`[Audio] 临时音频目录: ${AUDIO_TEMP_DIR}`);
console.log(`[Audio] 静态访问路径: /audio-temp/`);

const API_KEY = process.env.LIVEKIT_API_KEY || 'devkey';
const API_SECRET = process.env.LIVEKIT_API_SECRET || 'secret';

// LiveKit 房间服务客户端（用于查询房间状态）
const LIVEKIT_URL = process.env.LIVEKIT_URL || 'http://livekit-server:7880';
const roomService = new RoomServiceClient(LIVEKIT_URL, API_KEY, API_SECRET);

// ==================== 通义千问 AI 配置 (阿里云 DashScope) ====================
const DASHSCOPE_API_KEY = process.env.DASHSCOPE_API_KEY || 'sk-4835ac4f86254d9987dba055ea1a6e14';
const AI_MODEL = 'qwen-plus';  // 可选: qwen-turbo(快/便宜), qwen-plus(均衡), qwen-max(最强)

// 每个房间独立的 AI 对话历史 Map<roomName, SessionInfo>
const roomAISessions = new Map();

// AI 系统提示词 — 定义智能体的角色和能力
const AI_SYSTEM_PROMPT = `你是一个会议AI助手，名叫"小会"。你正在一个在线视频会议中为参会者提供帮助。

你的能力：
1. 回答参会者的各种知识性问题
2. 帮助总结会议讨论内容
3. 从对话中提取待办事项（Action Items）和负责人
4. 翻译内容（中英互译等）
5. 根据会议主题建议讨论议程
6. 解释技术概念或代码片段
7. 汇总各方观点，辅助决策

回复规则：
- 简洁、专业、友好
- 使用 Markdown 格式让内容结构清晰
- 如果是总结或提取待办，使用编号列表
- 回复语言与用户提问语言一致
- 不要编造会议中没有发生的事情`;

/**
 * 调用通义千问 API (OpenAI 兼容格式，国内直连)
 * @param {Array} messages - OpenAI 格式的消息 [{role, content}]
 * @param {string|null} systemPrompt - 系统提示词
 * @returns {string} AI 回复文本
 */
async function callAI(messages, systemPrompt = null) {
    const allMessages = [];
    if (systemPrompt) {
        allMessages.push({ role: 'system', content: systemPrompt });
    }
    allMessages.push(...messages);

    const postData = JSON.stringify({
        model: AI_MODEL,
        messages: allMessages,
        temperature: 0.7,
        max_tokens: 4096
    });

    return new Promise((resolve, reject) => {
        const options = {
            hostname: 'dashscope.aliyuncs.com',
            port: 443,
            path: '/compatible-mode/v1/chat/completions',
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Authorization': `Bearer ${DASHSCOPE_API_KEY}`,
                'Content-Length': Buffer.byteLength(postData)
            },
            timeout: 60000
        };

        const req = https.request(options, (res) => {
            let data = '';
            res.on('data', chunk => data += chunk);
            res.on('end', () => {
                try {
                    const json = JSON.parse(data);
                    if (res.statusCode !== 200) {
                        reject(new Error(`通义千问 API ${res.statusCode}: ${json.error?.message || data}`));
                        return;
                    }
                    if (!json.choices || !json.choices[0]) {
                        reject(new Error('通义千问返回空结果'));
                        return;
                    }
                    resolve(json.choices[0].message.content);
                } catch (e) {
                    reject(new Error(`解析响应失败: ${data.substring(0, 300)}`));
                }
            });
        });

        req.on('error', (e) => reject(new Error(`网络请求失败: ${e.message}`)));
        req.on('timeout', () => { req.destroy(); reject(new Error('请求超时(60s)')); });
        req.write(postData);
        req.end();
    });
}

// ==================== DashScope Paraformer-v2 离线 ASR ====================

/**
 * 向 DashScope 提交离线 ASR 转录任务
 * @param {string} fileUrl - 音频文件的公网 URL
 * @returns {string} task_id
 */
async function submitASRTask(fileUrl) {
    const postData = JSON.stringify({
        model: 'paraformer-v2',
        input: {
            file_urls: [fileUrl]
        },
        parameters: {
            language_hints: ['zh', 'en'],
            disfluency_removal_enabled: true   // 过滤语气词，让转录更干净
        }
    });

    return new Promise((resolve, reject) => {
        const options = {
            hostname: 'dashscope.aliyuncs.com',
            port: 443,
            path: '/api/v1/services/audio/asr/transcription',
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Authorization': `Bearer ${DASHSCOPE_API_KEY}`,
                'X-DashScope-Async': 'enable',       // 必须！异步任务
                'Content-Length': Buffer.byteLength(postData)
            },
            timeout: 30000
        };

        const req = https.request(options, (res) => {
            let data = '';
            res.on('data', chunk => data += chunk);
            res.on('end', () => {
                try {
                    const json = JSON.parse(data);
                    if (res.statusCode !== 200) {
                        reject(new Error(`ASR 提交失败 ${res.statusCode}: ${json.message || data.substring(0, 300)}`));
                        return;
                    }
                    const taskId = json.output?.task_id;
                    if (!taskId) {
                        reject(new Error('未获取到 task_id: ' + data.substring(0, 300)));
                        return;
                    }
                    console.log(`[ASR] 任务已提交: task_id=${taskId}`);
                    resolve(taskId);
                } catch (e) {
                    reject(new Error(`解析 ASR 提交响应失败: ${data.substring(0, 300)}`));
                }
            });
        });

        req.on('error', (e) => reject(new Error(`ASR 提交网络失败: ${e.message}`)));
        req.on('timeout', () => { req.destroy(); reject(new Error('ASR 提交超时')); });
        req.write(postData);
        req.end();
    });
}

/**
 * 查询 DashScope ASR 任务状态
 * @param {string} taskId - 任务 ID
 * @returns {object} { status, results }
 */
async function queryASRTask(taskId) {
    return new Promise((resolve, reject) => {
        const options = {
            hostname: 'dashscope.aliyuncs.com',
            port: 443,
            path: `/api/v1/tasks/${taskId}`,
            method: 'GET',
            headers: {
                'Authorization': `Bearer ${DASHSCOPE_API_KEY}`
            },
            timeout: 15000
        };

        const req = https.request(options, (res) => {
            let data = '';
            res.on('data', chunk => data += chunk);
            res.on('end', () => {
                try {
                    const json = JSON.parse(data);
                    if (res.statusCode !== 200) {
                        reject(new Error(`ASR 查询失败 ${res.statusCode}: ${data.substring(0, 300)}`));
                        return;
                    }
                    resolve({
                        status: json.output?.task_status,
                        results: json.output?.results,
                        raw: json
                    });
                } catch (e) {
                    reject(new Error(`解析 ASR 查询响应失败: ${data.substring(0, 300)}`));
                }
            });
        });

        req.on('error', (e) => reject(new Error(`ASR 查询网络失败: ${e.message}`)));
        req.on('timeout', () => { req.destroy(); reject(new Error('ASR 查询超时')); });
        req.end();
    });
}

/**
 * 轮询等待 ASR 任务完成
 * @param {string} taskId - 任务 ID
 * @param {number} maxWaitMs - 最大等待时间（毫秒），默认 5 分钟
 * @param {number} intervalMs - 轮询间隔（毫秒），默认 2 秒
 * @returns {array} results 数组
 */
async function waitForASRComplete(taskId, maxWaitMs = 300000, intervalMs = 2000) {
    const startTime = Date.now();
    while (Date.now() - startTime < maxWaitMs) {
        const result = await queryASRTask(taskId);
        console.log(`[ASR] 任务 ${taskId} 状态: ${result.status}`);

        if (result.status === 'SUCCEEDED') {
            return result.results;
        } else if (result.status === 'FAILED') {
            throw new Error('ASR 任务失败: ' + JSON.stringify(result.raw?.output || {}));
        }
        // PENDING 或 RUNNING，继续等待
        await new Promise(r => setTimeout(r, intervalMs));
    }
    throw new Error(`ASR 任务超时（等待 ${maxWaitMs / 1000} 秒）`);
}

/**
 * 从 DashScope ASR 结果中提取完整文本
 * @param {array} results - DashScope 返回的 results 数组
 * @returns {string} 合并后的转录文本
 */
async function extractTranscriptText(results) {
    if (!results || results.length === 0) {
        return '（无识别结果）';
    }

    let fullText = '';
    for (const result of results) {
        if (result.subtask_status !== 'SUCCEEDED') continue;
        const transcriptionUrl = result.transcription_url;
        if (!transcriptionUrl) continue;

        // 下载转录结果 JSON
        const transcriptJson = await fetchUrl(transcriptionUrl);
        const transcriptData = JSON.parse(transcriptJson);

        // 提取文本：transcripts[].text
        if (transcriptData.transcripts && Array.isArray(transcriptData.transcripts)) {
            for (const t of transcriptData.transcripts) {
                if (t.text) {
                    fullText += t.text + '\n';
                }
            }
        }
    }
    return fullText.trim() || '（无语音内容）';
}

/**
 * 通用 HTTP GET 获取 URL 内容
 * @param {string} url - 完整 URL
 * @returns {string} 响应内容
 */
function fetchUrl(url) {
    return new Promise((resolve, reject) => {
        const client = url.startsWith('https') ? https : http;
        client.get(url, { timeout: 30000 }, (res) => {
            // 处理重定向
            if (res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
                return fetchUrl(res.headers.location).then(resolve).catch(reject);
            }
            let data = '';
            res.on('data', chunk => data += chunk);
            res.on('end', () => resolve(data));
        }).on('error', reject).on('timeout', function () { this.destroy(); reject(new Error('下载超时')); });
    });
}

/**
 * 清理超过 1 小时的临时音频文件
 */
function cleanupTempAudioFiles() {
    try {
        const files = fs.readdirSync(AUDIO_TEMP_DIR);
        const now = Date.now();
        for (const file of files) {
            const filePath = path.join(AUDIO_TEMP_DIR, file);
            const stat = fs.statSync(filePath);
            if (now - stat.mtimeMs > 60 * 60 * 1000) { // 1 小时
                fs.unlinkSync(filePath);
                console.log(`[Audio] 清理临时文件: ${file}`);
            }
        }
    } catch (e) {
        console.error('[Audio] 清理临时文件失败:', e.message);
    }
}
setInterval(cleanupTempAudioFiles, 30 * 60 * 1000); // 每 30 分钟清理一次

/**
 * 获取或创建某个房间的 AI 聊天会话 (手动管理对话历史)
 */
function getOrCreateSession(roomName) {
    if (!roomAISessions.has(roomName)) {
        roomAISessions.set(roomName, {
            history: [],  // [{role: 'user', content: '...'}, {role: 'assistant', content: '...'}]
            createdAt: Date.now(),
            messageCount: 0
        });
    }
    return roomAISessions.get(roomName);
}

// 每小时清理超过 4 小时未活跃的会话，防止内存泄漏
setInterval(() => {
    const now = Date.now();
    for (const [room, session] of roomAISessions) {
        if (now - session.createdAt > 4 * 60 * 60 * 1000) {
            roomAISessions.delete(room);
            console.log(`[AI] 清理过期会话: ${room}`);
        }
    }
}, 60 * 60 * 1000);

const pool = mysql.createPool({
    host: process.env.DB_HOST || 'meeting_db',
    user: 'root',
    password: '200303',
    database: 'meeting_app',
    waitForConnections: true,
    connectionLimit: 10,
    queueLimit: 0
});

app.post('/api/register', async (req, res) => {
    const { username, password } = req.body;
    if (!username || !password) return res.status(400).json({ success: false });
    try {
        const hash = await bcrypt.hash(password, 10);
        await pool.execute('INSERT INTO users (username, password_hash) VALUES (?, ?)', [username, hash]);
        res.json({ success: true });
    } catch (e) {
        res.status(500).json({ success: false });
    }
});

app.post('/api/login', async (req, res) => {
    const { username, password } = req.body;
    try {
        const [rows] = await pool.execute('SELECT * FROM users WHERE username = ?', [username]);
        if (rows.length === 0 || !(await bcrypt.compare(password, rows[0].password_hash))) {
            return res.status(401).json({ success: false });
        }
        res.json({ success: true });
    } catch (e) {
        res.status(500).json({ success: false });
    }
});

app.post('/getToken', async (req, res) => {
    const { roomName, participantName, password } = req.body;
    try {
        const [users] = await pool.execute('SELECT id, password_hash FROM users WHERE username = ?', [participantName]);
        if (users.length === 0 || !(await bcrypt.compare(password, users[0].password_hash))) {
            return res.status(401).json({ error: 'unauthorized' });
        }
        const at = new AccessToken(API_KEY, API_SECRET, { identity: participantName });
        at.addGrant({ roomJoin: true, room: roomName, canPublish: true, canSubscribe: true });
        const token = at.toJwt();
        pool.execute('INSERT INTO meeting_logs (room_name, user_id, start_time) VALUES (?, ?, NOW())', [roomName, users[0].id]).catch(console.error);
        res.json({ success: true, token });
    } catch (e) {
        res.status(500).json({ error: 'server error' });
    }
});

app.get('/api/history', async (req, res) => {
    const { username } = req.query;
    try {
        const [rows] = await pool.execute(
            'SELECT l.id, l.room_name, l.start_time, l.duration_seconds FROM meeting_logs l JOIN users u ON l.user_id = u.id WHERE u.username = ? ORDER BY l.start_time DESC LIMIT 10',
            [username]
        );
        res.json({ success: true, history: rows });
    } catch (e) {
        res.status(500).json({ success: false });
    }
});

// ==================== AI 智能体接口 ====================

/**
 * POST /api/ai/chat
 * 向 AI 发送消息并获取回复
 * Body: { roomName, username, message }
 */
app.post('/api/ai/chat', async (req, res) => {
    const { roomName, username, message } = req.body;
    if (!roomName || !message) {
        return res.status(400).json({ success: false, error: '缺少 roomName 或 message' });
    }
    try {
        const session = getOrCreateSession(roomName);
        const prompt = username ? `[${username}]: ${message}` : message;

        // 将用户消息加入历史
        session.history.push({ role: 'user', content: prompt });

        // 调用 DeepSeek API，带上完整对话历史
        const reply = await callAI(session.history, AI_SYSTEM_PROMPT);

        // 将 AI 回复也加入历史
        session.history.push({ role: 'assistant', content: reply });
        session.messageCount++;

        // 防止历史过长，保留最近 40 轮对话
        if (session.history.length > 80) {
            session.history = session.history.slice(-60);
        }

        console.log(`[AI] room=${roomName} user=${username} count=${session.messageCount}`);
        res.json({ success: true, reply });
    } catch (e) {
        console.error('[AI] chat error:', e.message);
        // 如果调用失败，移除刚加入的用户消息
        const session = roomAISessions.get(roomName);
        if (session && session.history.length > 0 && session.history[session.history.length - 1].role === 'user') {
            session.history.pop();
        }
        res.status(500).json({ success: false, error: 'AI 服务暂时不可用: ' + e.message });
    }
});

/**
 * POST /api/ai/summarize
 * 将聊天记录发给 AI 生成会议纪要
 * Body: { roomName, chatHistory: [{ sender, content, time }] }
 */
app.post('/api/ai/summarize', async (req, res) => {
    const { roomName, chatHistory } = req.body;
    if (!roomName || !chatHistory || !Array.isArray(chatHistory)) {
        return res.status(400).json({ success: false, error: '参数错误' });
    }
    try {
        const transcript = chatHistory
            .map(m => `[${m.time}] ${m.sender}: ${m.content}`)
            .join('\n');
        const prompt = `请根据以下会议聊天记录，生成一份结构化的会议纪要。
包含：会议主要议题、关键讨论点、达成的结论、待办事项（标明负责人）。

聊天记录：
${transcript}`;

        const contents = [{ role: 'user', content: prompt }];
        const summary = await callAI(contents);
        res.json({ success: true, summary });
    } catch (e) {
        console.error('[AI] summarize error:', e.message);
        res.status(500).json({ success: false, error: 'AI 服务暂时不可用: ' + e.message });
    }
});

/**
 * POST /api/ai/clear
 * 清除某房间的 AI 对话历史
 * Body: { roomName }
 */
app.post('/api/ai/clear', async (req, res) => {
    const { roomName } = req.body;
    if (roomAISessions.has(roomName)) {
        roomAISessions.delete(roomName);
        console.log(`[AI] 手动清除会话: ${roomName}`);
    }
    res.json({ success: true, message: 'AI 对话历史已清除' });
});

/**
 * POST /api/ai/transcribe
 * 语音转文字 — 使用 DashScope Paraformer-v2 离线 ASR
 * Body: { roomName, audioData (base64 WAV), username? }
 * 
 * 流程：
 * 1. 接收客户端 base64 编码的 WAV 数据
 * 2. 保存为临时文件并通过静态目录提供公网 URL
 * 3. 提交到 DashScope Paraformer-v2 进行离线转录
 * 4. 轮询等待转录完成
 * 5. 下载并解析转录结果，返回完整文本
 */
app.post('/api/ai/transcribe', async (req, res) => {
    const { roomName, audioData, username } = req.body;
    if (!audioData) {
        return res.status(400).json({ success: false, error: '缺少 audioData (base64 WAV)' });
    }
    try {
        // 1. 将 base64 WAV 保存为临时文件
        const fileName = `asr_${Date.now()}_${crypto.randomUUID().substring(0, 8)}.wav`;
        const filePath = path.join(AUDIO_TEMP_DIR, fileName);
        const audioBuffer = Buffer.from(audioData, 'base64');
        fs.writeFileSync(filePath, audioBuffer);

        const fileSizeMB = (audioBuffer.length / 1024 / 1024).toFixed(2);
        console.log(`[ASR] 收到音频: room=${roomName} user=${username} file=${fileName} size=${fileSizeMB}MB`);

        // 2. 构建公网可访问的 URL
        //    注意：需要服务器有公网 IP 或域名。
        //    这里使用请求中的 Host 头来自动适配
        const host = req.headers.host || `localhost:${port}`;
        const protocol = req.protocol || 'http';
        const fileUrl = `${protocol}://${host}/audio-temp/${fileName}`;
        console.log(`[ASR] 音频文件 URL: ${fileUrl}`);

        // 3. 提交 DashScope ASR 任务
        const taskId = await submitASRTask(fileUrl);
        console.log(`[ASR] 任务提交成功: task_id=${taskId}`);

        // 4. 轮询等待任务完成（最长等 5 分钟）
        const results = await waitForASRComplete(taskId, 300000, 2000);
        console.log(`[ASR] 任务完成: task_id=${taskId}`);

        // 5. 提取转录文本
        const transcript = await extractTranscriptText(results);
        console.log(`[ASR] 转录结果: ${transcript.substring(0, 200)}`);

        // 6. 清理临时文件（延迟清理，给 DashScope 足够时间下载）
        setTimeout(() => {
            try { fs.unlinkSync(filePath); } catch (e) { /* ignore */ }
        }, 5 * 60 * 1000);  // 5 分钟后清理

        res.json({ success: true, transcript });
    } catch (e) {
        console.error('[ASR] transcribe error:', e.message);
        res.status(500).json({ success: false, error: '语音识别失败: ' + e.message });
    }
});

/**
 * POST /api/ai/meeting-minutes
 * 综合生成会议纪要 — 结合语音转录文本 + 聊天记录
 * Body: { roomName, transcripts: [{ time, speaker, text }], chatHistory: [{ sender, content, time }] }
 */
app.post('/api/ai/meeting-minutes', async (req, res) => {
    const { roomName, transcripts, chatHistory } = req.body;
    if (!roomName) {
        return res.status(400).json({ success: false, error: '缺少 roomName' });
    }
    try {
        let prompt = '请根据以下会议内容，生成一份专业、结构化的会议纪要。\n';
        prompt += '要求包含：1)会议概要 2)主要议题讨论 3)关键决议 4)待办事项(标明负责人和截止时间) 5)下次会议安排(如有提及)\n\n';

        // 拼接语音转录内容
        if (transcripts && Array.isArray(transcripts) && transcripts.length > 0) {
            prompt += '=== 会议语音转录 ===\n';
            prompt += transcripts.map(t => `[${t.time}] ${t.speaker}: ${t.text}`).join('\n');
            prompt += '\n\n';
        }

        // 拼接聊天记录
        if (chatHistory && Array.isArray(chatHistory) && chatHistory.length > 0) {
            prompt += '=== 会议聊天记录 ===\n';
            prompt += chatHistory.map(m => `[${m.time}] ${m.sender}: ${m.content}`).join('\n');
            prompt += '\n';
        }

        if ((!transcripts || transcripts.length === 0) && (!chatHistory || chatHistory.length === 0)) {
            return res.status(400).json({ success: false, error: '没有可用的会议内容' });
        }

        const contents = [{ role: 'user', content: prompt }];
        const minutes = await callAI(contents);
        res.json({ success: true, minutes });
    } catch (e) {
        console.error('[AI] meeting-minutes error:', e.message);
        res.status(500).json({ success: false, error: '会议纪要生成失败: ' + e.message });
    }
});

// ==================== 房间状态查询 ====================

/**
 * GET /api/room/status?roomName=xxx
 * 查询 LiveKit 房间是否还在活跃状态
 */
app.get('/api/room/status', async (req, res) => {
    const { roomName } = req.query;
    if (!roomName) {
        return res.status(400).json({ success: false, error: '缺少 roomName' });
    }
    try {
        const rooms = await roomService.listRooms([roomName]);
        const room = rooms.find(r => r.name === roomName);
        if (room) {
            res.json({ success: true, active: true, participantCount: room.numParticipants || 0 });
        } else {
            res.json({ success: true, active: false, participantCount: 0 });
        }
    } catch (e) {
        console.error('[Room] status check error:', e.message);
        // 如果 LiveKit 服务不可达，默认允许尝试加入
        res.json({ success: true, active: true, participantCount: -1 });
    }
});

// ==================== 删除会议历史 ====================

/**
 * POST /api/history/delete
 * 删除指定的会议历史记录
 * Body: { id, username }
 */
app.post('/api/history/delete', async (req, res) => {
    const { id, username } = req.body;
    if (!id || !username) {
        return res.status(400).json({ success: false, error: '缺少参数' });
    }
    try {
        const [result] = await pool.execute(
            'DELETE FROM meeting_logs WHERE id = ? AND user_id = (SELECT id FROM users WHERE username = ?)',
            [id, username]
        );
        console.log(`[History] 删除记录 id=${id} user=${username} affected=${result.affectedRows}`);
        res.json({ success: true, deleted: result.affectedRows > 0 });
    } catch (e) {
        console.error('[History] delete error:', e.message);
        res.status(500).json({ success: false, error: '删除失败: ' + e.message });
    }
});

// ==================== HTTP 服务器 ====================

app.listen(port, () => console.log(`running on ${port}`));
