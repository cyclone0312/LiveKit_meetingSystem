/**
 * @file livekitmanager.cpp
 * @brief LiveKit 连接管理器实现（使用官方 C++ SDK）
 */

#include "livekitmanager.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

// ==================== 配置常量 ====================
namespace Config
{
    // 请将这里的 IP 地址替换为你的阿里云服务器公网 IP
    const QString DEFAULT_LIVEKIT_SERVER = "ws://8.162.3.195:7880";
    const QString DEFAULT_TOKEN_SERVER = "http://8.162.3.195:3000";
}

// ==================== 静态成员初始化 ====================
bool LiveKitManager::s_sdkInitialized = false;

// ==================== LiveKitRoomDelegate 实现 ====================

void LiveKitRoomDelegate::onParticipantConnected(livekit::Room &room,
                                                 const livekit::ParticipantConnectedEvent &event)
{
    Q_UNUSED(room)
    if (manager)
    {
        QString identity = QString::fromStdString(event.participant->identity());
        QString name = QString::fromStdString(event.participant->name());
        qDebug() << "[LiveKit] 参会者加入:" << identity << name;
        emit manager->participantJoined(identity, name.isEmpty() ? identity : name);
    }
}

void LiveKitRoomDelegate::onParticipantDisconnected(livekit::Room &room,
                                                    const livekit::ParticipantDisconnectedEvent &event)
{
    Q_UNUSED(room)
    if (manager && event.participant)
    {
        QString identity = QString::fromStdString(event.participant->identity());
        qDebug() << "[LiveKit] 参会者离开:" << identity;
        emit manager->participantLeft(identity);
    }
}

void LiveKitRoomDelegate::onTrackSubscribed(livekit::Room &room,
                                            const livekit::TrackSubscribedEvent &event)
{
    Q_UNUSED(room)
    if (manager)
    {
        QString identity = QString::fromStdString(event.participant->identity());
        QString trackSid = QString::fromStdString(event.track->sid());
        int kind = static_cast<int>(event.track->kind());
        qDebug() << "[LiveKit] 轨道订阅:" << identity << trackSid << "kind:" << kind;
        emit manager->trackSubscribed(identity, trackSid, kind);
    }
}

void LiveKitRoomDelegate::onTrackUnsubscribed(livekit::Room &room,
                                              const livekit::TrackUnsubscribedEvent &event)
{
    Q_UNUSED(room)
    if (manager && event.participant && event.track)
    {
        QString identity = QString::fromStdString(event.participant->identity());
        QString trackSid = QString::fromStdString(event.track->sid());
        qDebug() << "[LiveKit] 轨道取消订阅:" << identity << trackSid;
        emit manager->trackUnsubscribed(identity, trackSid);
    }
}

void LiveKitRoomDelegate::onTrackMuted(livekit::Room &room,
                                       const livekit::TrackMutedEvent &event)
{
    Q_UNUSED(room)
    if (manager && event.participant && event.publication)
    {
        QString identity = QString::fromStdString(event.participant->identity());
        QString trackSid = QString::fromStdString(event.publication->sid());
        qDebug() << "[LiveKit] 轨道静音:" << identity << trackSid;
        emit manager->trackMuted(identity, trackSid, true);
    }
}

void LiveKitRoomDelegate::onTrackUnmuted(livekit::Room &room,
                                         const livekit::TrackUnmutedEvent &event)
{
    Q_UNUSED(room)
    if (manager && event.participant && event.publication)
    {
        QString identity = QString::fromStdString(event.participant->identity());
        QString trackSid = QString::fromStdString(event.publication->sid());
        qDebug() << "[LiveKit] 轨道取消静音:" << identity << trackSid;
        emit manager->trackMuted(identity, trackSid, false);
    }
}

void LiveKitRoomDelegate::onDisconnected(livekit::Room &room,
                                         const livekit::DisconnectedEvent &event)
{
    Q_UNUSED(room)
    Q_UNUSED(event)
    if (manager)
    {
        qDebug() << "[LiveKit] 断开连接";
        manager->m_isConnected = false;
        manager->m_isConnecting = false;
        emit manager->connectionStateChanged();
        emit manager->disconnected();
    }
}

void LiveKitRoomDelegate::onReconnecting(livekit::Room &room,
                                         const livekit::ReconnectingEvent &event)
{
    Q_UNUSED(room)
    Q_UNUSED(event)
    if (manager)
    {
        qDebug() << "[LiveKit] 正在重连...";
        emit manager->reconnecting();
    }
}

void LiveKitRoomDelegate::onReconnected(livekit::Room &room,
                                        const livekit::ReconnectedEvent &event)
{
    Q_UNUSED(room)
    Q_UNUSED(event)
    if (manager)
    {
        qDebug() << "[LiveKit] 重连成功";
        emit manager->reconnected();
    }
}

void LiveKitRoomDelegate::onUserPacketReceived(livekit::Room &room,
                                               const livekit::UserDataPacketEvent &event)
{
    Q_UNUSED(room)
    if (manager)
    {
        QString identity = event.participant ? QString::fromStdString(event.participant->identity()) : QString();
        QByteArray data(reinterpret_cast<const char *>(event.data.data()),
                        static_cast<int>(event.data.size()));
        qDebug() << "[LiveKit] 收到数据:" << identity << data.size() << "bytes";
        emit manager->dataReceived(identity, data);

        // 尝试解析为聊天消息
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isNull() && doc.isObject())
        {
            QJsonObject obj = doc.object();
            if (obj.contains("type") && obj["type"].toString() == "chat")
            {
                QString senderName = obj["senderName"].toString();
                QString content = obj["content"].toString();
                emit manager->chatMessageReceived(identity, senderName, content);
            }
        }
    }
}

// ==================== LiveKitManager 实现 ====================

LiveKitManager::LiveKitManager(QObject *parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this)), m_room(std::make_unique<livekit::Room>()), m_delegate(std::make_unique<LiveKitRoomDelegate>()), m_serverUrl(Config::DEFAULT_LIVEKIT_SERVER), m_tokenServerUrl(Config::DEFAULT_TOKEN_SERVER), m_isConnected(false), m_isConnecting(false)
{
    // 初始化 LiveKit SDK（只需要一次）
    if (!s_sdkInitialized)
    {
        livekit::initialize(livekit::LogSink::kConsole);
        s_sdkInitialized = true;
        qDebug() << "[LiveKitManager] SDK 初始化完成";
    }

    // 设置委托
    m_delegate->manager = this;
    m_room->setDelegate(m_delegate.get());

    qDebug() << "[LiveKitManager] 初始化完成";
    qDebug() << "[LiveKitManager] LiveKit Server:" << m_serverUrl;
    qDebug() << "[LiveKitManager] Token Server:" << m_tokenServerUrl;
}

LiveKitManager::~LiveKitManager()
{
    if (m_isConnected)
    {
        leaveRoom();
    }

    // 注意：不要在这里调用 livekit::shutdown()
    // 因为可能还有其他 LiveKitManager 实例
}

// ==================== 属性 Getter ====================

bool LiveKitManager::isConnected() const { return m_isConnected; }
bool LiveKitManager::isConnecting() const { return m_isConnecting; }
QString LiveKitManager::currentRoom() const { return m_currentRoom; }
QString LiveKitManager::currentUser() const { return m_currentUser; }
QString LiveKitManager::serverUrl() const { return m_serverUrl; }
QString LiveKitManager::tokenServerUrl() const { return m_tokenServerUrl; }
QString LiveKitManager::errorMessage() const { return m_errorMessage; }

// ==================== 属性 Setter ====================

void LiveKitManager::setServerUrl(const QString &url)
{
    if (m_serverUrl != url)
    {
        m_serverUrl = url;
        emit serverUrlChanged();
    }
}

void LiveKitManager::setTokenServerUrl(const QString &url)
{
    if (m_tokenServerUrl != url)
    {
        m_tokenServerUrl = url;
        emit tokenServerUrlChanged();
    }
}

// ==================== 核心方法 ====================

void LiveKitManager::joinRoom(const QString &roomName, const QString &userName)
{
    qDebug() << "[LiveKitManager] 准备加入房间:" << roomName << "用户:" << userName;

    if (roomName.isEmpty() || userName.isEmpty())
    {
        setError("房间名和用户名不能为空");
        return;
    }

    if (m_isConnecting)
    {
        setError("正在连接中，请稍候...");
        return;
    }

    if (m_isConnected)
    {
        leaveRoom();
    }

    m_pendingRoom = roomName;
    m_pendingUser = userName;
    m_isConnecting = true;
    emit connectionStateChanged();

    // 请求 Token
    requestToken(roomName, userName);
}

void LiveKitManager::leaveRoom()
{
    qDebug() << "[LiveKitManager] 离开房间:" << m_currentRoom;

    // 重新创建 Room 对象（SDK 没有 disconnect 方法，需要重新创建）
    m_room = std::make_unique<livekit::Room>();
    m_room->setDelegate(m_delegate.get());

    m_isConnected = false;
    m_isConnecting = false;
    m_currentRoom.clear();
    m_currentUser.clear();
    m_currentToken.clear();

    emit connectionStateChanged();
    emit currentRoomChanged();
    emit currentUserChanged();
    emit disconnected();
}

void LiveKitManager::setMicrophoneMuted(bool muted)
{
    if (!m_isConnected || !m_room)
        return;

    auto *localParticipant = m_room->localParticipant();
    if (localParticipant)
    {
        // TODO: 实现麦克风静音
        qDebug() << "[LiveKitManager] 设置麦克风静音:" << muted;
    }
}

void LiveKitManager::setCameraMuted(bool muted)
{
    if (!m_isConnected || !m_room)
        return;

    auto *localParticipant = m_room->localParticipant();
    if (localParticipant)
    {
        // TODO: 实现摄像头静音
        qDebug() << "[LiveKitManager] 设置摄像头静音:" << muted;
    }
}

void LiveKitManager::sendData(const QByteArray &data)
{
    if (!m_isConnected || !m_room)
    {
        qWarning() << "[LiveKitManager] 未连接，无法发送数据";
        return;
    }

    auto *localParticipant = m_room->localParticipant();
    if (localParticipant)
    {
        std::vector<uint8_t> dataVec(data.begin(), data.end());
        // TODO: 调用 SDK 的发送数据方法
        qDebug() << "[LiveKitManager] 发送数据:" << data.size() << "bytes";
    }
}

// ==================== 兼容旧接口 ====================

void LiveKitManager::sendChatMessage(const QString &message)
{
    QJsonObject chatMsg;
    chatMsg["type"] = "chat";
    chatMsg["senderName"] = m_currentUser;
    chatMsg["content"] = message;

    QByteArray data = QJsonDocument(chatMsg).toJson(QJsonDocument::Compact);
    sendData(data);
}

void LiveKitManager::updateMicState(bool enabled)
{
    setMicrophoneMuted(!enabled);
}

void LiveKitManager::updateCameraState(bool enabled)
{
    setCameraMuted(!enabled);
}

void LiveKitManager::updateScreenShareState(bool enabled)
{
    Q_UNUSED(enabled)
    // TODO: 实现屏幕共享
}

void LiveKitManager::updateHandRaiseState(bool raised)
{
    Q_UNUSED(raised)
    // 通过数据消息发送举手状态
    QJsonObject msg;
    msg["type"] = "handRaise";
    msg["raised"] = raised;
    sendData(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

// ==================== 内部方法 ====================

void LiveKitManager::requestToken(const QString &roomName, const QString &userName)
{
    qDebug() << "[LiveKitManager] 正在请求 Token...";

    QUrl url(m_tokenServerUrl + "/getToken");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject requestBody;
    requestBody["roomName"] = roomName;
    requestBody["participantName"] = userName;

    QByteArray jsonData = QJsonDocument(requestBody).toJson(QJsonDocument::Compact);

    QNetworkReply *reply = m_networkManager->post(request, jsonData);

    connect(reply, &QNetworkReply::finished, this, [this, reply]()
            {
        if (reply->error() != QNetworkReply::NoError) {
            QString errorStr = QString("Token 请求失败: %1").arg(reply->errorString());
            qWarning() << "[LiveKitManager]" << errorStr;
            setError(errorStr);
            m_isConnecting = false;
            emit connectionStateChanged();
            emit tokenRequestFailed(errorStr);
            reply->deleteLater();
            return;
        }
        
        QByteArray responseData = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        
        if (doc.isNull() || !doc.isObject()) {
            setError("Token 响应格式错误");
            m_isConnecting = false;
            emit connectionStateChanged();
            reply->deleteLater();
            return;
        }
        
        QString token = doc.object()["token"].toString();
        
        if (token.isEmpty()) {
            setError("未获取到有效的 Token");
            m_isConnecting = false;
            emit connectionStateChanged();
            reply->deleteLater();
            return;
        }
        
        qDebug() << "[LiveKitManager] 成功获取 Token";
        emit tokenReceived(token);
        
        m_currentToken = token;
        connectToRoom(token);
        
        reply->deleteLater(); });
}

void LiveKitManager::connectToRoom(const QString &token)
{
    qDebug() << "[LiveKitManager] 正在连接 LiveKit Server...";
    qDebug() << "[LiveKitManager] URL:" << m_serverUrl;

    // 配置连接选项
    livekit::RoomOptions options;
    options.auto_subscribe = true; // 自动订阅远程轨道
    options.dynacast = true;       // 启用动态码率

    // 连接到房间
    std::string url = m_serverUrl.toStdString();
    std::string tokenStr = token.toStdString();

    bool success = m_room->Connect(url, tokenStr, options);

    if (success)
    {
        qDebug() << "[LiveKitManager] 连接成功!";
        m_isConnected = true;
        m_isConnecting = false;
        m_currentRoom = m_pendingRoom;
        m_currentUser = m_pendingUser;

        emit connectionStateChanged();
        emit currentRoomChanged();
        emit currentUserChanged();
        emit connected();
    }
    else
    {
        qWarning() << "[LiveKitManager] 连接失败";
        setError("连接 LiveKit Server 失败");
        m_isConnecting = false;
        emit connectionStateChanged();
    }
}

void LiveKitManager::setError(const QString &error)
{
    m_errorMessage = error;
    emit errorOccurred(error);
    qWarning() << "[LiveKitManager] 错误:" << error;
}
