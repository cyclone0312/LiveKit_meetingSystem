const express = require('express');
const { AccessToken } = require('livekit-server-sdk');
const cors = require('cors');
const mysql = require('mysql2/promise'); // 新增：数据库驱动
const bcrypt = require('bcrypt');       // 新增：加密工具

const app = express();
const port = 3000;

app.use(cors());
app.use(express.json());

// --- 配置区 ---
const API_KEY = process.env.LIVEKIT_API_KEY || 'devkey';
const API_SECRET = process.env.LIVEKIT_API_SECRET || 'secret';

// 数据库连接配置 (对应你 docker-compose 里的设置)
const dbConfig = {
    host: process.env.DB_HOST || 'meeting_db',
    user: 'root',
    password: 'your_password_here', // ！！！这里填你之前在yml里写的密码
    database: 'meeting_app'
};

// --- 新增接口：用户注册 ---
app.post('/api/register', async (req, res) => {
    const { username, password } = req.body;
    try {
        const connection = await mysql.createConnection(dbConfig);
        // 对密码进行加盐哈希加密，不存明文
        const passwordHash = await bcrypt.hash(password, 10); 
        await connection.execute(
            'INSERT INTO users (username, password_hash) VALUES (?, ?)',
            [username, passwordHash]
        );
        await connection.end();
        res.json({ success: true, message: '注册成功' });
    } catch (error) {
        res.status(500).json({ success: false, error: '注册失败，用户名可能已存在' });
    }
});

// --- 核心改造：带登录校验的 Token 获取接口 ---
app.post('/getToken', async (req, res) => {
    // 1. 获取参数 (新增了 password)
    const { roomName, participantName, password } = req.body;

    if (!roomName || !participantName || !password) {
        return res.status(400).json({ error: '用户名、密码、房间号都得填！' });
    }

    try {
        // 2. 数据库校验身份
        const connection = await mysql.createConnection(dbConfig);
        const [rows] = await connection.execute('SELECT * FROM users WHERE username = ?', [participantName]);
        
        if (rows.length === 0) {
            return res.status(401).json({ error: '用户不存在' });
        }

        const user = rows[0];
        // 校验密码是否匹配
        const isPasswordValid = await bcrypt.compare(password, user.password_hash);
        if (!isPasswordValid) {
            return res.status(401).json({ error: '密码错误' });
        }

        // 3. 校验通过，开始你原来的 LiveKit Token 生成逻辑
        const at = new AccessToken(API_KEY, API_SECRET, {
            identity: participantName,
            ttl: '10m',
        });

        at.addGrant({
            roomJoin: true,
            room: roomName,
            canPublish: true,
            canSubscribe: true,
        });

        const token = at.toJwt();

        // 4. 新增：在数据库记录一笔“谁进了哪个房”
        await connection.execute(
            'INSERT INTO meeting_logs (room_name, user_id) VALUES (?, ?)',
            [roomName, user.id]
        );

        await connection.end();
        console.log(`验证通过：用户 ${participantName} 进入了 ${roomName}`);
        
        res.json({ token });

    } catch (error) {
        console.error(error);
        res.status(500).json({ error: '服务器内部错误' });
    }
});

app.listen(port, () => {
    console.log(`API Server (带数据库增强) 启动啦！监听端口: ${port}`);
});