const express = require('express');
const { AccessToken } = require('livekit-server-sdk');
const cors = require('cors');
const mysql = require('mysql2/promise');
const bcrypt = require('bcrypt');
const { GoogleGenerativeAI } = require('@google/generative-ai');

const app = express();
const port = 3000;

app.use(cors());
app.use(express.json({ limit: '50mb' }));
app.use(express.urlencoded({ limit: '50mb', extended: true }));

const API_KEY = process.env.LIVEKIT_API_KEY || 'devkey';
const API_SECRET = process.env.LIVEKIT_API_SECRET || 'secret';

// ==================== Google Gemini AI 配置 ====================
const GEMINI_API_KEY = process.env.GEMINI_API_KEY || 'AIzaSyA4BDao-SbfjdlG0sJeMZZ1yCqHnxehNag'; // 部署时用环境变量或直接替换
const genAI = new GoogleGenerativeAI(GEMINI_API_KEY);

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
 * 获取或创建某个房间的 AI 聊天会话
 */
function getOrCreateSession(roomName) {
    if (!roomAISessions.has(roomName)) {
        const model = genAI.getGenerativeModel({ model: 'gemini-2.0-flash-exp' });
        const chat = model.startChat({
            history: [],
            systemInstruction: AI_SYSTEM_PROMPT,
        });
        roomAISessions.set(roomName, {
            chat,
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
        const token = await at.toJwt();
        pool.execute('INSERT INTO meeting_logs (room_name, user_id, start_time) VALUES (?, ?, NOW())', [roomName, users[0].id]).catch(console.error);
        res.json({ success: true, token });
    } catch (e) {
        res.status(500).json({ error: 'server error' });
    }
});

app.get('/history', async (req, res) => {
    const { username } = req.query;
    try {
        const [rows] = await pool.execute(
            'SELECT l.room_name, l.start_time FROM meeting_logs l JOIN users u ON l.user_id = u.id WHERE u.username = ? ORDER BY l.start_time DESC LIMIT 10',
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
        // 把发送者信息附到消息里，让 AI 知道谁在说话
        const prompt = username ? `[${username}]: ${message}` : message;
        const result = await session.chat.sendMessage(prompt);
        const reply = result.response.text();
        session.messageCount++;
        console.log(`[AI] room=${roomName} user=${username} count=${session.messageCount}`);
        res.json({ success: true, reply });
    } catch (e) {
        console.error('[AI] chat error:', e.message);
        res.status(500).json({ success: false, error: 'AI 服务暂时不可用' });
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
        const model = genAI.getGenerativeModel({ model: 'gemini-2.0-flash' });
        const transcript = chatHistory
            .map(m => `[${m.time}] ${m.sender}: ${m.content}`)
            .join('\n');
        const prompt = `请根据以下会议聊天记录，生成一份结构化的会议纪要。
包含：会议主要议题、关键讨论点、达成的结论、待办事项（标明负责人）。

聊天记录：
${transcript}`;
        const result = await model.generateContent(prompt);
        const summary = result.response.text();
        res.json({ success: true, summary });
    } catch (e) {
        console.error('[AI] summarize error:', e.message);
        res.status(500).json({ success: false, error: 'AI 服务暂时不可用' });
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
 * 语音转文字 — 将音频数据发给 Gemini 进行语音识别
 * Body: { roomName, audioData (base64), mimeType? }
 * 
 * 客户端将麦克风录制的音频编码为 base64 后发到此接口
 * 支持的音频格式: audio/wav, audio/mp3, audio/webm, audio/ogg 等
 */
app.post('/api/ai/transcribe', async (req, res) => {
    const { roomName, audioData, mimeType } = req.body;
    if (!audioData) {
        return res.status(400).json({ success: false, error: '缺少 audioData' });
    }
    try {
        const model = genAI.getGenerativeModel({ model: 'gemini-2.0-flash' });
        const result = await model.generateContent([
            {
                inlineData: {
                    mimeType: mimeType || 'audio/wav',
                    data: audioData  // base64 编码的音频数据
                }
            },
            { text: '请将以上音频内容精确地转录为文字。只输出转录的文字内容，不要添加任何多余说明。如果有多人说话，请用"发言者1："、"发言者2："等标注区分。' }
        ]);
        const transcript = result.response.text();
        console.log(`[AI] transcribe room=${roomName} length=${transcript.length}`);
        res.json({ success: true, transcript });
    } catch (e) {
        console.error('[AI] transcribe error:', e.message);
        res.status(500).json({ success: false, error: '语音识别失败' });
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
        const model = genAI.getGenerativeModel({ model: 'gemini-2.0-flash' });

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

        const result = await model.generateContent(prompt);
        const minutes = result.response.text();
        res.json({ success: true, minutes });
    } catch (e) {
        console.error('[AI] meeting-minutes error:', e.message);
        res.status(500).json({ success: false, error: '会议纪要生成失败' });
    }
});

app.listen(port, () => console.log(`running on ${port}`));
