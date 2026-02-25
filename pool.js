const express = require('express');
const { AccessToken } = require('livekit-server-sdk');
const cors = require('cors');
const mysql = require('mysql2/promise');
const bcrypt = require('bcrypt');

const app = express();
const port = 3000;

app.use(cors());
app.use(express.json());

// --- 配置区 ---
const API_KEY = process.env.LIVEKIT_API_KEY || 'devkey';
const API_SECRET = process.env.LIVEKIT_API_SECRET || 'secret';

// 1. 创建连接池
const pool = mysql.createPool({
    host: process.env.DB_HOST || 'meeting_db',
    user: 'root',
    password: '200303', // 确认与 docker-compose.yml 一致
    database: 'meeting_app',
    waitForConnections: true,
    connectionLimit: 10, // 连接池最大连接数
    queueLimit: 0
});

// --- 1. 用户注册接口 ---
app.post('/api/register', async (req, res) => {
    const { username, password } = req.body;
    if (!username || !password) return res.status(400).json({ success: false, error: '参数不全' });

    try {
        const passwordHash = await bcrypt.hash(password, 10);
        // 直接从 pool 执行，它会自动获取并释放连接
        await pool.execute(
            'INSERT INTO users (username, password_hash) VALUES (?, ?)',
            [username, passwordHash]
        );
        res.json({ success: true, message: '注册成功' });
    } catch (error) {
        res.status(500).json({ success: false, error: '注册失败，用户名可能已存在' });
    }
});

// --- 2. 登录接口 ---
app.post('/api/login', async (req, res) => {
    const { username, password } = req.body;
    try {
        const [rows] = await pool.execute('SELECT * FROM users WHERE username = ?', [username]);
        
        if (rows.length === 0 || !(await bcrypt.compare(password, rows[0].password_hash))) {
            return res.status(401).json({ success: false, message: '用户名或密码错误' });
        }
        res.json({ success: true, message: '登录成功' });
    } catch (error) {
        res.status(500).json({ success: false, error: '服务器内部错误' });
    }
});

// --- 3. 获取 Token 接口 ---
app.post('/api/getToken', async (req, res) => {
    const { roomName, participantName, password } = req.body;

    // 1. 基础参数检查
    if (!roomName || !participantName || !password) {
        return res.status(400).json({ error: '请提供完整的房间名、用户名和密码' });
    }

    try {
        // 2. 身份验证：发放 Token 前先校验数据库中的密码
        // 使用 pool.execute 从连接池获取连接并执行查询
        const [users] = await pool.execute(
            'SELECT id, password_hash FROM users WHERE username = ?', 
            [participantName]
        );

        if (users.length === 0 || !(await bcrypt.compare(password, users[0].password_hash))) {
            return res.status(401).json({ error: '身份校验失败，无法获取 Token' });
        }

        const userId = users[0].id;

        // 3. 生成 LiveKit Access Token
        // 设置 identity 为用户名，并赋予加入指定房间的权限
        const at = new AccessToken(API_KEY, API_SECRET, { 
            identity: participantName,
            name: participantName // 可选：显示名称
        });

        at.addGrant({ 
            roomJoin: true, 
            room: roomName,
            canPublish: true,  // 允许推流（发视频/语音）
            canSubscribe: true // 允许拉流（看别人）
        });

        const token = at.toJwt();

        // 4. 异步记录会议开始日志
        // 这里的执行不会阻塞 Token 的返回，提高响应速度
        pool.execute(
            'INSERT INTO meeting_logs (room_name, user_id, start_time) VALUES (?, ?, NOW())',
            [roomName, userId]
        ).catch(err => console.error("日志记录失败:", err));

        // 5. 返回 Token 给客户端 (Qt/QML 或 Web)
        res.json({ 
            success: true,
            token: token 
        });

    } catch (error) {
        console.error("getToken Error:", error);
        res.status(500).json({ error: '服务器内部错误' });
    }
});

// --- 4. 获取历史会议记录接口 ---
app.get('/api/history', async (req, res) => {
    const { username } = req.query;
    if (!username) return res.status(400).json({ success: false, error: '缺少用户名' });

    try {
        const [rows] = await pool.execute(
            `SELECT l.room_name, l.start_time, l.duration_seconds 
             FROM meeting_logs l
             JOIN users u ON l.user_id = u.id
             WHERE u.username = ?
             ORDER BY l.start_time DESC LIMIT 10`,
            [username]
        );
        res.json({ success: true, history: rows });
    } catch (error) {
        res.status(500).json({ success: false, error: error.message });
    }
});

app.listen(port, () => {
    console.log(`Server running at http://localhost:${port}`);
});