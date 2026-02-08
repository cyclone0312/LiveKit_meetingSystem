// ============================================================================
// 模块导入区（类似 C# 的 using 或 Qt 的 #include）
// ============================================================================

// require() 是 Node.js 导入模块的方式（类似 C# 的 using 或 Qt 的 #include）
// const 声明常量，一旦赋值不能再改变（类似 C# 的 const）

const express = require('express');  // 导入 express 框架，用于创建 Web 服务器（类似 C# 的 ASP.NET Core）
const { AccessToken } = require('livekit-server-sdk');  // 从 livekit SDK 中导入 AccessToken 类（花括号表示解构赋值）
const cors = require('cors');  // 导入跨域资源共享中间件，允许前端跨域访问这个服务器
const mysql = require('mysql2/promise');  // 导入 MySQL 数据库驱动，/promise 表示使用 Promise 异步版本
const bcrypt = require('bcrypt');  // 导入密码加密库，用于安全地存储密码（类似 C# 的 BCrypt.Net）

// ============================================================================
// 应用初始化区
// ============================================================================

const app = express();  // 创建一个 express 应用实例（类似 C# 中 new WebApplication()）
const port = 3000;  // 定义服务器监听的端口号（类似 Qt 的 QTcpServer::listen(port)）

// app.use() 注册中间件（middleware），在每个请求到达路由处理函数前先经过这些中间件处理
app.use(cors());  // 启用 CORS，允许浏览器从其他域名访问这个服务器
app.use(express.json());  // 启用 JSON 解析器，自动将请求体中的 JSON 字符串解析为 JavaScript 对象（类似 C# 的 [FromBody]）

// ============================================================================
// 配置区：API 密钥和数据库配置
// ============================================================================

// process.env 是 Node.js 的环境变量对象（类似 C# 的 Environment.GetEnvironmentVariable()）
// || 是逻辑或运算符，这里用于提供默认值：如果环境变量不存在，就使用 || 后面的值
const API_KEY = process.env.LIVEKIT_API_KEY || 'devkey';  // LiveKit 的 API 密钥
const API_SECRET = process.env.LIVEKIT_API_SECRET || 'secret';  // LiveKit 的 API 密钥（用于签名 Token）

// JavaScript 对象字面量（类似 C# 的匿名对象或 Qt 的 QVariantMap）
// 数据库连接配置对象
const dbConfig = {
    host: process.env.DB_HOST || 'meeting_db',  // 数据库服务器地址
    user: 'root',  // 数据库用户名
    password: 'your_password_here',  // ！！！数据库密码，实际使用时需要修改
    database: 'meeting_app'  // 要连接的数据库名称
};

// ============================================================================
// API 路由 1：用户注册接口
// ============================================================================

// app.post() 定义一个 POST 请求的路由（类似 C# MVC 的 [HttpPost] 或 Qt 的 HTTP 请求处理）
// '/api/register' 是接口的 URL 路径
// async 关键字表示这是一个异步函数（类似 C# 的 async）
// (req, res) 是箭头函数语法（类似 C# 的 lambda 表达式 (req, res) => {...}）
//   req = request（请求对象，包含客户端发来的数据）
//   res = response（响应对象，用于向客户端返回数据）
app.post('/api/register', async (req, res) => {
    
    // 解构赋值：从 req.body 对象中提取 username 和 password 属性
    // req.body 包含客户端通过 POST 请求发送的 JSON 数据（已被 express.json() 中间件解析）
    // 相当于：const username = req.body.username; const password = req.body.password;
    const { username, password } = req.body;
    
    // try-catch 异常处理（和 C# 完全一样）
    try {
        // await 关键字等待异步操作完成（类似 C# 的 await）
        // mysql.createConnection() 创建数据库连接，返回 Promise（类似 C# 的 Task）
        const connection = await mysql.createConnection(dbConfig);
        
        // bcrypt.hash() 对密码进行加密
        // 参数1：原始密码
        // 参数2：盐值轮次（10 表示加密强度，数字越大越安全但越慢）
        // 返回加密后的密码哈希值（不可逆，安全存储）
        const passwordHash = await bcrypt.hash(password, 10);
        
        // connection.execute() 执行 SQL 语句
        // 第一个参数：SQL 语句，? 是占位符（防止 SQL 注入攻击）
        // 第二个参数：数组，按顺序替换 SQL 中的 ? 占位符
        await connection.execute(
            'INSERT INTO users (username, password_hash) VALUES (?, ?)',  // SQL 插入语句
            [username, passwordHash]  // 实际值数组：第一个 ? 替换为 username，第二个 ? 替换为 passwordHash
        );
        
        // 关闭数据库连接（类似 C# 的 connection.Close() 或 Qt 的数据库关闭）
        await connection.end();
        
        // res.json() 向客户端返回 JSON 格式的响应（类似 C# MVC 的 return Json()）
        res.json({ success: true, message: '注册成功' });
        
    } catch (error) {  // 捕获异常（变量名 error 包含错误信息）
        
        // res.status(500) 设置 HTTP 状态码为 500（服务器内部错误）
        // 然后返回 JSON 错误信息
        res.status(500).json({ success: false, error: '注册失败，用户名可能已存在' });
    }
});

// ============================================================================
// API 路由 2：获取会议 Token 接口（带登录验证）
// ============================================================================

app.post('/getToken', async (req, res) => {
    
    // --- 步骤 1：从请求体中提取参数 ---
    // 解构赋值：提取房间名、参与者名（用户名）、密码
    const { roomName, participantName, password } = req.body;

    // 参数验证：确保三个参数都存在
    // !roomName 表示 roomName 为 null、undefined 或空字符串时为 true
    // || 是逻辑或，任何一个条件为 true 则整个表达式为 true
    if (!roomName || !participantName || !password) {
        // return 提前结束函数执行
        // res.status(400) 返回 HTTP 400 状态码（客户端请求错误）
        return res.status(400).json({ error: '用户名、密码、房间号都得填！' });
    }

    try {
        // --- 步骤 2：连接数据库并验证用户身份 ---
        
        // 创建数据库连接
        const connection = await mysql.createConnection(dbConfig);
        
        // 执行 SQL 查询，根据用户名查找用户
        // connection.execute() 返回一个数组：[结果集, 字段信息]
        // [rows] 使用数组解构只取第一个元素（结果集）
        const [rows] = await connection.execute(
            'SELECT * FROM users WHERE username = ?',  // 查询 SQL
            [participantName]  // 用 participantName 替换 ? 占位符
        );
        
        // 检查查询结果：如果 rows 数组长度为 0，说明用户不存在
        if (rows.length === 0) {
            return res.status(401).json({ error: '用户不存在' });  // 401 = 未授权
        }

        // rows[0] 取数组第一个元素（查到的用户记录）
        // 在 JavaScript 中，数组下标从 0 开始（和 C#、Qt 一样）
        const user = rows[0];
        
        // bcrypt.compare() 比较明文密码和数据库中的密码哈希值
        // 参数1：用户输入的明文密码
        // 参数2：数据库中存储的加密后的密码哈希
        // 返回 true（密码正确）或 false（密码错误）
        const isPasswordValid = await bcrypt.compare(password, user.password_hash);
        
        // 密码验证失败
        if (!isPasswordValid) {
            return res.status(401).json({ error: '密码错误' });
        }

        // --- 步骤 3：生成 LiveKit 访问令牌（Token） ---
        
        // 创建 AccessToken 对象（类似 C# 的 new AccessToken()）
        // 参数1、2：API 密钥和密钥（用于签名）
        // 参数3：对象字面量，包含令牌配置
        const at = new AccessToken(API_KEY, API_SECRET, {
            identity: participantName,  // 用户身份标识
            ttl: '2h',  // Token 有效期（2 小时），ttl = Time To Live - 足够覆盖大部分会议场景
        });

        // 为 Token 添加权限授权
        // addGrant() 是 AccessToken 类的方法
        at.addGrant({
            roomJoin: true,       // 允许加入房间
            room: roomName,       // 指定可以加入的房间名
            canPublish: true,     // 允许发布音视频流
            canSubscribe: true,   // 允许订阅（接收）其他人的音视频流
        });

        // 将 Token 对象转换为 JWT 字符串格式
        // JWT = JSON Web Token，一种加密的令牌格式
        const token = at.toJwt();

        // --- 步骤 4：记录会议日志到数据库 ---
        
        // 向 meeting_logs 表插入一条记录，记录谁进入了哪个房间
        // user.id 是从数据库查出的用户 ID（在步骤 2 中获取）
        await connection.execute(
            'INSERT INTO meeting_logs (room_name, user_id) VALUES (?, ?)',
            [roomName, user.id]  // 插入房间名和用户 ID
        );

        // 关闭数据库连接
        await connection.end();
        
        // console.log() 在控制台输出日志（类似 C# 的 Console.WriteLine 或 Qt 的 qDebug()）
        // 反引号 `` 和 ${} 是模板字符串语法（类似 C# 的 $"字符串{变量}"）
        console.log(`验证通过：用户 ${participantName} 进入了 ${roomName}`);
        
        // 向客户端返回生成的 Token
        res.json({ token });  // 等价于 { token: token }，ES6 简写语法

    } catch (error) {  // 捕获所有异常
        
        // console.error() 在控制台输出错误信息（红色显示）
        console.error(error);
        
        // 返回 500 错误给客户端
        res.status(500).json({ error: '服务器内部错误' });
    }
});

// ============================================================================
// 启动服务器
// ============================================================================

// app.listen() 启动服务器，监听指定端口
// 参数1：端口号
// 参数2：回调函数，服务器启动成功后执行（类似 C# 的 Action 委托）
app.listen(port, () => {
    console.log(`API Server (带数据库增强) 启动啦！监听端口: ${port}`);
});

// ============================================================================
// 补充说明：JavaScript 与 C#/Qt 的主要区别
// ============================================================================
// 1. 变量声明：
//    - const: 常量（类似 C# const）
//    - let: 变量（类似 C# var，但有块级作用域）
//    - var: 旧式变量声明（不推荐使用）
//
// 2. 异步编程：
//    - async/await: 和 C# 完全一样
//    - Promise: 类似 C# 的 Task
//
// 3. 函数定义：
//    - function name() {...}: 普通函数（类似 C# 的方法）
//    - () => {...}: 箭头函数（类似 C# 的 lambda 表达式）
//
// 4. 对象：
//    - {key: value}: 对象字面量（类似 C# 的匿名对象）
//    - 不需要定义类，可以直接创建对象
//
// 5. 类型：
//    - JavaScript 是动态类型语言，变量不需要声明类型
//    - 运行时自动判断类型（和 C# 的 var 类似，但更灵活）
//
// 6. 解构赋值：
//    - const {a, b} = obj; 从对象中提取属性
//    - const [x, y] = arr; 从数组中提取元素
//
// 7. 模板字符串：
//    - 使用反引号 `` 而不是双引号 ""
//    - 使用 ${变量} 插入变量值
//
// 8. 箭头函数的 this：
//    - 箭头函数不绑定自己的 this，继承外层作用域的 this
//    - 普通函数有自己的 this（类似 C# 的实例方法）
// ============================================================================
