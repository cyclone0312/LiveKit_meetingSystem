/**
 * @file livekitmanager.cpp
 * @brief LiveKit 连接管理器实现（使用官方 C++ SDK）
 */

#include "livekitmanager.h"
#include "mediacapture.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThread>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>

// ==================== 配置常量 ====================
namespace Config {
// 请将这里的 IP 地址替换为你的阿里云服务器公网 IP
const QString DEFAULT_LIVEKIT_SERVER = "ws://8.162.3.195:7880";
const QString DEFAULT_TOKEN_SERVER = "http://8.162.3.195:3000";
} // namespace Config

// ==================== 静态成员初始化 ====================
bool LiveKitManager::s_sdkInitialized = false;

// ==================== LiveKitRoomDelegate 实现 ====================

void LiveKitRoomDelegate::onParticipantConnected(
    livekit::Room &room, const livekit::ParticipantConnectedEvent &event) {
  Q_UNUSED(room)
  // 【关键修复】检查 enabled 标志，在房间转换期间忽略回调
  if (!enabled.load() || !manager) {
    qDebug() << "[LiveKit] 忽略 onParticipantConnected 回调 (enabled="
             << enabled.load() << ")";
    return;
  }
  QString identity = QString::fromStdString(event.participant->identity());
  QString name = QString::fromStdString(event.participant->name());
  qDebug() << "[LiveKit] 参会者加入:" << identity << name;
  emit manager->participantJoined(identity, name.isEmpty() ? identity : name);
}

void LiveKitRoomDelegate::onParticipantDisconnected(
    livekit::Room &room, const livekit::ParticipantDisconnectedEvent &event) {
  Q_UNUSED(room)
  // 【关键修复】检查 enabled 标志
  if (!enabled.load() || !manager || !event.participant) {
    return;
  }
  QString identity = QString::fromStdString(event.participant->identity());
  qDebug() << "[LiveKit] 参会者离开:" << identity;
  emit manager->participantLeft(identity);
}

void LiveKitRoomDelegate::onTrackSubscribed(
    livekit::Room &room, const livekit::TrackSubscribedEvent &event) {
  Q_UNUSED(room)
  // 【关键修复】检查 enabled 标志
  if (!enabled.load() || !manager) {
    qDebug() << "[LiveKit] 忽略 onTrackSubscribed 回调 (enabled="
             << enabled.load() << ")";
    return;
  }
  // 保存 manager 到本地变量，以便在 lambda 中捕获
  LiveKitManager *mgr = manager;

  QString identity = QString::fromStdString(event.participant->identity());
  QString trackSid = QString::fromStdString(event.track->sid());
  livekit::TrackKind kind = event.track->kind();
  int kindInt = static_cast<int>(kind);
  qDebug() << "[LiveKit] 轨道订阅:" << identity << trackSid
           << "kind:" << kindInt;
  emit mgr->trackSubscribed(identity, trackSid, kindInt);

  // 根据轨道类型创建相应的渲染器/播放器
  if (kind == livekit::TrackKind::KIND_VIDEO) {
    // 创建远程视频渲染器
    // 【关键修复】必须在主线程创建 RemoteVideoRenderer
    // Qt 对象和视频 Sink 操作需要在主线程进行
    qDebug() << "[LiveKit] 创建远程视频渲染器:" << identity;

    auto track = event.track;
    QMetaObject::invokeMethod(
        mgr,
        [mgr, track, identity]() {
          auto renderer = std::make_shared<RemoteVideoRenderer>(track);
          renderer->setParticipantId(identity);
          renderer->start();
          mgr->m_remoteVideoRenderers[identity] = renderer;

          // 【关键】检查是否有待处理的 VideoSink（QML
          // 在渲染器创建前就设置了）
          if (mgr->m_pendingVideoSinks.contains(identity)) {
            QVideoSink *pendingSink = mgr->m_pendingVideoSinks.take(identity);
            // 【安全检查】QPointer 可能在 QML 对象销毁后变为 null
            if (pendingSink) {
              qDebug() << "[LiveKit] 应用待处理的 VideoSink:" << identity;
              renderer->setExternalVideoSink(pendingSink);
            } else {
              qDebug() << "[LiveKit] 待处理的 VideoSink 已失效，跳过:"
                       << identity;
            }
          }
          qDebug() << "[LiveKit] 视频渲染器已在主线程创建并启动:" << identity;
        },
        Qt::QueuedConnection);
  } else if (kind == livekit::TrackKind::KIND_AUDIO) {
    // 创建远程音频播放器
    // 【关键修复】必须在主线程创建 RemoteAudioPlayer
    // 因为 Qt 信号槽的 QueuedConnection 需要对象的线程关联正确
    // 如果在回调线程创建，信号将无法传递到主线程的事件循环
    qDebug() << "[LiveKit] 创建远程音频播放器:" << identity;

    auto track = event.track;
    QMetaObject::invokeMethod(
        mgr,
        [mgr, track, identity]() {
          auto player = std::make_shared<RemoteAudioPlayer>(track);
          player->setParticipantId(identity);
          player->start();
          mgr->m_remoteAudioPlayers[identity] = player;
          qDebug() << "[LiveKit] 音频播放器已在主线程创建并启动:" << identity;
        },
        Qt::QueuedConnection);
  }
}

void LiveKitRoomDelegate::onTrackUnsubscribed(
    livekit::Room &room, const livekit::TrackUnsubscribedEvent &event) {
  Q_UNUSED(room)
  // 【关键修复】检查 enabled 标志
  if (!enabled.load() || !manager || !event.participant || !event.track) {
    return;
  }
  // 保存 manager 到本地变量，以便在 lambda 中捕获
  LiveKitManager *mgr = manager;

  QString identity = QString::fromStdString(event.participant->identity());
  QString trackSid = QString::fromStdString(event.track->sid());
  livekit::TrackKind kind = event.track->kind();
  qDebug() << "[LiveKit] 轨道取消订阅:" << identity << trackSid;
  emit mgr->trackUnsubscribed(identity, trackSid);

  // 清理对应的渲染器/播放器
  if (kind == livekit::TrackKind::KIND_VIDEO) {
    if (mgr->m_remoteVideoRenderers.contains(identity)) {
      qDebug() << "[LiveKit] 停止并移除远程视频渲染器:" << identity;
      auto renderer = mgr->m_remoteVideoRenderers.take(identity);
      if (renderer) {
        renderer->stop();
      }
      // 通知 QML 视频 Sink 已移除
      QMetaObject::invokeMethod(
          mgr,
          [mgr, identity]() { emit mgr->remoteVideoSinkRemoved(identity); },
          Qt::QueuedConnection);
    }
  } else if (kind == livekit::TrackKind::KIND_AUDIO) {
    if (mgr->m_remoteAudioPlayers.contains(identity)) {
      qDebug() << "[LiveKit] 停止并移除远程音频播放器:" << identity;
      auto player = mgr->m_remoteAudioPlayers.take(identity);
      if (player) {
        player->stop();
      }
    }
  }
}

void LiveKitRoomDelegate::onTrackMuted(livekit::Room &room,
                                       const livekit::TrackMutedEvent &event) {
  Q_UNUSED(room)
  // 【关键修复】检查 enabled 标志
  if (!enabled.load() || !manager || !event.participant || !event.publication) {
    return;
  }
  QString identity = QString::fromStdString(event.participant->identity());
  QString trackSid = QString::fromStdString(event.publication->sid());
  qDebug() << "[LiveKit] 轨道静音:" << identity << trackSid;
  emit manager->trackMuted(identity, trackSid, true);

  // 如果是音频轨道，通知对应的 RemoteAudioPlayer 暂停处理
  if (manager->m_remoteAudioPlayers.contains(identity)) {
    auto player = manager->m_remoteAudioPlayers[identity];
    if (player) {
      player->setMuted(true);
    }
  }
}

void LiveKitRoomDelegate::onTrackUnmuted(
    livekit::Room &room, const livekit::TrackUnmutedEvent &event) {
  Q_UNUSED(room)
  // 【关键修复】检查 enabled 标志
  if (!enabled.load() || !manager || !event.participant || !event.publication) {
    return;
  }
  QString identity = QString::fromStdString(event.participant->identity());
  QString trackSid = QString::fromStdString(event.publication->sid());
  qDebug() << "[LiveKit] 轨道取消静音:" << identity << trackSid;
  emit manager->trackMuted(identity, trackSid, false);

  // 如果是音频轨道，通知对应的 RemoteAudioPlayer 恢复处理
  if (manager->m_remoteAudioPlayers.contains(identity)) {
    auto player = manager->m_remoteAudioPlayers[identity];
    if (player) {
      player->setMuted(false);
    }
  }
}

void LiveKitRoomDelegate::onDisconnected(
    livekit::Room &room, const livekit::DisconnectedEvent &event) {
  Q_UNUSED(room)
  Q_UNUSED(event)
  // 【关键修复】检查 enabled 标志
  if (!enabled.load()) {
    qDebug() << "[LiveKit] 忽略 onDisconnected 回调 (delegate 已禁用)";
    return;
  }
  if (manager && manager->m_isConnected) {
    qDebug() << "[LiveKit] 断开连接";
    manager->m_isConnected = false;
    manager->m_isConnecting = false;
    emit manager->connectionStateChanged();
    emit manager->disconnected();
  } else {
    qDebug() << "[LiveKit] 断开连接回调（已忽略，因为正在清理）";
  }
}

void LiveKitRoomDelegate::onReconnecting(
    livekit::Room &room, const livekit::ReconnectingEvent &event) {
  Q_UNUSED(room)
  Q_UNUSED(event)
  // 【关键修复】检查 enabled 标志
  if (!enabled.load() || !manager) {
    return;
  }
  qDebug() << "[LiveKit] 正在重连...";
  emit manager->reconnecting();
}

void LiveKitRoomDelegate::onReconnected(
    livekit::Room &room, const livekit::ReconnectedEvent &event) {
  Q_UNUSED(room)
  Q_UNUSED(event)
  // 【关键修复】检查 enabled 标志
  if (!enabled.load() || !manager) {
    return;
  }
  qDebug() << "[LiveKit] 重连成功";
  emit manager->reconnected();
}

void LiveKitRoomDelegate::onUserPacketReceived(
    livekit::Room &room, const livekit::UserDataPacketEvent &event) {
  Q_UNUSED(room)
  // 【关键修复】检查 enabled 标志
  if (!enabled.load() || !manager) {
    return;
  }
  QString identity = event.participant
                         ? QString::fromStdString(event.participant->identity())
                         : QString();
  QByteArray data(reinterpret_cast<const char *>(event.data.data()),
                  static_cast<int>(event.data.size()));
  qDebug() << "[LiveKit] 收到数据:" << identity << data.size() << "bytes";
  emit manager->dataReceived(identity, data);

  // 尝试解析为聊天消息
  QJsonDocument doc = QJsonDocument::fromJson(data);
  if (!doc.isNull() && doc.isObject()) {
    QJsonObject obj = doc.object();
    if (obj.contains("type") && obj["type"].toString() == "chat") {
      QString senderName = obj["senderName"].toString();
      QString content = obj["content"].toString();
      emit manager->chatMessageReceived(identity, senderName, content);
    }
  }
}

// ==================== LiveKitManager 实现 ====================

LiveKitManager::LiveKitManager(QObject *parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this)),
      m_room(std::make_unique<livekit::Room>()),
      m_delegate(std::make_unique<LiveKitRoomDelegate>()),
      m_mediaCapture(std::make_unique<MediaCapture>(this)),
      m_screenCapture(std::make_unique<ScreenCapture>(this)),
      m_serverUrl(Config::DEFAULT_LIVEKIT_SERVER),
      m_tokenServerUrl(Config::DEFAULT_TOKEN_SERVER), m_isConnected(false),
      m_isConnecting(false) {
  // 初始化 LiveKit SDK（只需要一次）
  if (!s_sdkInitialized) {
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

LiveKitManager::~LiveKitManager() {
  if (m_isConnected) {
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

bool LiveKitManager::isCameraPublished() const { return m_cameraPublished; }
bool LiveKitManager::isMicrophonePublished() const {
  return m_microphonePublished;
}
bool LiveKitManager::isScreenSharePublished() const {
  return m_screenSharePublished;
}
MediaCapture *LiveKitManager::mediaCapture() const {
  return m_mediaCapture.get();
}
ScreenCapture *LiveKitManager::screenCapture() const {
  return m_screenCapture.get();
}

// ==================== 属性 Setter ====================

void LiveKitManager::setServerUrl(const QString &url) {
  if (m_serverUrl != url) {
    m_serverUrl = url;
    emit serverUrlChanged();
  }
}

void LiveKitManager::setTokenServerUrl(const QString &url) {
  if (m_tokenServerUrl != url) {
    m_tokenServerUrl = url;
    emit tokenServerUrlChanged();
  }
}

// ==================== 核心方法 ====================

void LiveKitManager::joinRoom(const QString &roomName,
                              const QString &userName) {
  qDebug() << "[LiveKitManager] 准备加入房间:" << roomName
           << "用户:" << userName;

  if (roomName.isEmpty() || userName.isEmpty()) {
    setError("房间名和用户名不能为空");
    return;
  }

  if (m_isConnecting) {
    setError("正在连接中，请稍候...");
    return;
  }

  if (m_isConnected) {
    // 如果已连接，先离开旧房间
    leaveRoom();

    // 【重要】使用延时确保 SDK 内部完全清理后再连接新房间
    // 【关键修复】增加延迟到 500ms，给 SDK 后台线程足够时间完成事件派发和清理
    // 如果延迟太短，旧 Room 的事件回调可能与新 Room 的初始化产生冲突
    m_pendingRoom = roomName;
    m_pendingUser = userName;
    m_isConnecting = true;
    emit connectionStateChanged();

    // 【关键修复】延迟后重新连接
    QTimer::singleShot(500, this, [this]() {
      qDebug() << "[LiveKitManager] 延时后请求新房间 Token";
      requestToken(m_pendingRoom, m_pendingUser);
    });
    return;
  }

  m_pendingRoom = roomName;
  m_pendingUser = userName;
  m_isConnecting = true;
  emit connectionStateChanged();

  // 请求 Token
  requestToken(roomName, userName);
}

void LiveKitManager::leaveRoom() {
  qDebug() << "[LiveKitManager] 离开房间:" << m_currentRoom;

  // 先停止本地媒体采集
  if (m_mediaCapture) {
    m_mediaCapture->stopCamera();
    m_mediaCapture->stopMicrophone();
  }

  // 清除发布状态
  m_videoPublication.reset();
  m_audioPublication.reset();
  m_screenSharePublication.reset();
  m_cameraPublished = false;
  m_microphonePublished = false;
  m_screenSharePublished = false;
  emit cameraPublishedChanged();
  emit microphonePublishedChanged();
  emit screenSharePublishedChanged();

  // 停止屏幕共享
  if (m_screenCapture) {
    m_screenCapture->stopCapture();
  }

  // 停止并清除所有远程渲染器
  qDebug() << "[LiveKitManager] 停止所有远程视频渲染器:"
           << m_remoteVideoRenderers.count();
  for (auto &renderer : m_remoteVideoRenderers) {
    if (renderer) {
      renderer->stop();
    }
  }
  m_remoteVideoRenderers.clear();

  qDebug() << "[LiveKitManager] 停止所有远程音频播放器:"
           << m_remoteAudioPlayers.count();
  for (auto &player : m_remoteAudioPlayers) {
    if (player) {
      player->stop();
    }
  }
  m_remoteAudioPlayers.clear();

  // 清除待处理的 VideoSink 缓存
  m_pendingVideoSinks.clear();

  // 【重要】在销毁旧 Room 之前，先禁用 delegate 并移除
  // 避免销毁过程中触发回调导致崩溃
  if (m_delegate) {
    // 【关键修复】禁用 delegate，让所有回调立即返回
    // 这比销毁 delegate 更安全，不会导致 SDK 内部内存问题
    m_delegate->enabled.store(false);
    qDebug() << "[LiveKitManager] Delegate 已禁用";
  }
  if (m_room) {
    m_room->setDelegate(nullptr);
  }

  // 给 SDK 后台线程时间来完成当前正在处理的事件
  qDebug() << "[LiveKitManager] 等待 SDK 事件处理完成...";
  QThread::msleep(100);

  // 重新创建 Room 对象（SDK 没有 disconnect 方法，需要重新创建）
  // 旧的 Room 会在这里被销毁，触发 SDK 内部的断开连接流程
  qDebug() << "[LiveKitManager] 销毁旧 Room...";
  m_room.reset();

  // 给 SDK 更多时间完成内部清理
  qDebug() << "[LiveKitManager] 等待 SDK 完全清理 (500ms)...";
  QThread::msleep(500);

  // 创建新的 Room 对象
  qDebug() << "[LiveKitManager] 创建新 Room...";
  m_room = std::make_unique<livekit::Room>();

  // 【关键修复】先不启用 delegate，等到连接成功后才启用
  // 这可以避免旧的 SDK 事件在连接过程中触发回调
  // delegate 的 enabled 仍然是 false
  m_room->setDelegate(m_delegate.get());

  m_isConnected = false;
  m_isConnecting = false;
  m_currentRoom.clear();
  m_currentUser.clear();
  m_currentToken.clear();

  // 【关键修复】完全重置 LiveKit 源和轨道
  // 使用 resetLiveKitSources 代替单独的 recreateVideoTrack/recreateAudioTrack
  // 这会销毁旧的 Source 对象，确保后台线程不再持有引用
  if (m_mediaCapture) {
    qDebug() << "[LiveKitManager] 重置 LiveKit 源和轨道...";
    m_mediaCapture->resetLiveKitSources();
  }
  if (m_screenCapture) {
    m_screenCapture->resetLiveKitSources();
  }

  emit connectionStateChanged();
  emit currentRoomChanged();
  emit currentUserChanged();
  emit disconnected();
}

void LiveKitManager::setMicrophoneMuted(bool muted) {
  if (!m_isConnected || !m_room)
    return;

  auto *localParticipant = m_room->localParticipant();
  if (localParticipant) {
    // TODO: 实现麦克风静音
    qDebug() << "[LiveKitManager] 设置麦克风静音:" << muted;
  }
}

void LiveKitManager::setCameraMuted(bool muted) {
  if (!m_isConnected || !m_room)
    return;

  auto *localParticipant = m_room->localParticipant();
  if (localParticipant) {
    // TODO: 实现摄像头静音
    qDebug() << "[LiveKitManager] 设置摄像头静音:" << muted;
  }
}

void LiveKitManager::sendData(const QByteArray &data) {
  if (!m_isConnected || !m_room) {
    qWarning() << "[LiveKitManager] 未连接，无法发送数据";
    return;
  }

  auto *localParticipant = m_room->localParticipant();
  if (localParticipant) {
    std::vector<uint8_t> dataVec(data.begin(), data.end());
    // TODO: 调用 SDK 的发送数据方法
    qDebug() << "[LiveKitManager] 发送数据:" << data.size() << "bytes";
  }
}

// ==================== 兼容旧接口 ====================

void LiveKitManager::sendChatMessage(const QString &message) {
  QJsonObject chatMsg;
  chatMsg["type"] = "chat";
  chatMsg["senderName"] = m_currentUser;
  chatMsg["content"] = message;

  QByteArray data = QJsonDocument(chatMsg).toJson(QJsonDocument::Compact);
  sendData(data);
}

void LiveKitManager::updateMicState(bool enabled) {
  setMicrophoneMuted(!enabled);
}

void LiveKitManager::updateCameraState(bool enabled) {
  setCameraMuted(!enabled);
}

void LiveKitManager::updateScreenShareState(bool enabled) {
  if (enabled) {
    publishScreenShare();
  } else {
    unpublishScreenShare();
  }
}

void LiveKitManager::updateHandRaiseState(bool raised) {
  Q_UNUSED(raised)
  // 通过数据消息发送举手状态
  QJsonObject msg;
  msg["type"] = "handRaise";
  msg["raised"] = raised;
  sendData(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

// ==================== 内部方法 ====================

void LiveKitManager::requestToken(const QString &roomName,
                                  const QString &userName) {
  qDebug() << "[LiveKitManager] 正在请求 Token...";

  QUrl url(m_tokenServerUrl + "/getToken");
  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  QJsonObject requestBody;
  requestBody["roomName"] = roomName;
  requestBody["participantName"] = userName;

  QByteArray jsonData =
      QJsonDocument(requestBody).toJson(QJsonDocument::Compact);

  QNetworkReply *reply = m_networkManager->post(request, jsonData);

  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    if (reply->error() != QNetworkReply::NoError) {
      QString errorStr =
          QString("Token 请求失败: %1").arg(reply->errorString());
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

    reply->deleteLater();
  });
}

void LiveKitManager::connectToRoom(const QString &token) {
  qDebug() << "[LiveKitManager] 正在连接 LiveKit Server...";
  qDebug() << "[LiveKitManager] URL:" << m_serverUrl;

  // 配置连接选项
  livekit::RoomOptions options;
  options.auto_subscribe = true; // 自动订阅远程轨道
  options.dynacast = true;       // 启用动态码率

  // 连接到房间
  std::string url = m_serverUrl.toStdString();
  std::string tokenStr = token.toStdString();

  // 【重要】Connect() 是阻塞调用，会阻塞主线程导致 UI 卡死
  // 将连接操作放到后台线程执行，避免阻塞 UI
  QtConcurrent::run([this, url, tokenStr, options]() {
    qDebug() << "[LiveKitManager] 后台线程开始连接...";

    // 在 Connect 前短暂等待，确保 SDK 内部状态稳定
    QThread::msleep(50);

    // 【安全检查】确保 Room 对象仍然有效
    if (!m_room) {
      qWarning() << "[LiveKitManager] Room 对象已失效，取消连接";
      QMetaObject::invokeMethod(
          this,
          [this]() {
            m_isConnecting = false;
            emit connectionStateChanged();
          },
          Qt::QueuedConnection);
      return;
    }

    bool success = m_room->Connect(url, tokenStr, options);

    // 连接完成后，通过 QMetaObject::invokeMethod 回到主线程处理结果
    QMetaObject::invokeMethod(
        this,
        [this, success]() {
          if (success) {
            qDebug() << "[LiveKitManager] 连接成功!";

            // 【关键修复】连接成功后才启用 delegate
            // 这确保了旧的 SDK 事件不会在连接过程中触发回调
            if (m_delegate) {
              m_delegate->enabled.store(true);
              qDebug() << "[LiveKitManager] Delegate 已重新启用";
            }

            m_isConnected = true;
            m_isConnecting = false;
            m_currentRoom = m_pendingRoom;
            m_currentUser = m_pendingUser;

            emit connectionStateChanged();
            emit currentRoomChanged();
            emit currentUserChanged();
            emit connected();

            // 【关键】连接成功后，遍历已存在的远程参会者并发送信号
            // 这样后加入的用户也能看到已经在房间里的参会者
            if (m_room) {
              auto existingParticipants = m_room->remoteParticipants();
              qDebug() << "[LiveKitManager] 已有远程参会者数量:"
                       << existingParticipants.size();

              for (const auto &participant : existingParticipants) {
                if (participant) {
                  QString identity =
                      QString::fromStdString(participant->identity());
                  QString name = QString::fromStdString(participant->name());
                  if (name.isEmpty())
                    name = identity;

                  qDebug() << "[LiveKitManager] 发现已有参会者:" << identity
                           << name;
                  emit participantJoined(identity, name);

                  // TODO: 如果已有参会者有视频轨道，也需要创建渲染器
                  // 这需要遍历 participant 的 trackPublications
                }
              }
            }
          } else {
            qWarning() << "[LiveKitManager] 连接失败";
            setError("连接 LiveKit Server 失败");
            m_isConnecting = false;
            emit connectionStateChanged();
          }
        },
        Qt::QueuedConnection);
  });
}

void LiveKitManager::setError(const QString &error) {
  m_errorMessage = error;
  emit errorOccurred(error);
  qWarning() << "[LiveKitManager] 错误:" << error;
}

// ==================== 轨道发布控制 ====================
// 【设计说明】
// 使用 mute/unmute 控制摄像头开关，而不是 publish/unpublish
// 原因：unpublishTrack 是阻塞调用，在某些情况下会卡死
// 轨道只在首次开启摄像头时发布一次，之后通过 mute 控制

void LiveKitManager::publishCamera() {
  qDebug() << "[LiveKitManager] publishCamera 被调用, isConnected="
           << m_isConnected << "cameraPublished=" << m_cameraPublished;

  if (!m_isConnected) {
    setError("未连接到房间，无法发布摄像头");
    return;
  }

  // 如果已经发布过，只需要 unmute 和启动本地摄像头
  if (m_cameraPublished && m_videoPublication) {
    qDebug() << "[LiveKitManager] 摄像头已发布，启动本地摄像头并 unmute";

    // 先启动本地摄像头
    if (!m_mediaCapture->isCameraActive()) {
      m_mediaCapture->startCamera();
    }

    try {
      m_videoPublication->setMuted(false);
      qDebug() << "[LiveKitManager] 摄像头已 unmute";
    } catch (const std::exception &e) {
      qWarning() << "[LiveKitManager] unmute 失败:" << e.what();
    }
    return;
  }

  // 首次发布：先启动本地摄像头
  if (!m_mediaCapture->isCameraActive()) {
    qDebug() << "[LiveKitManager] 首次发布，启动本地摄像头...";
    m_mediaCapture->startCamera();

    // 等待摄像头启动完成后再发布
    QTimer::singleShot(200, this, [this]() { doPublishCameraTrack(); });
    return;
  }

  // 摄像头已经启动，直接发布
  doPublishCameraTrack();
}

void LiveKitManager::doPublishCameraTrack() {
  qDebug() << "[LiveKitManager] doPublishCameraTrack 被调用";

  if (!m_isConnected || m_cameraPublished) {
    qDebug() << "[LiveKitManager] 跳过发布 (未连接或已发布)";
    return;
  }

  auto *localParticipant = m_room->localParticipant();
  if (!localParticipant) {
    setError("无法获取本地参会者");
    return;
  }

  try {
    auto videoTrack = m_mediaCapture->getVideoTrack();
    if (!videoTrack) {
      setError("视频轨道未创建");
      return;
    }

    qDebug() << "[LiveKitManager] 正在发布视频轨道...";

    // 设置发布选项
    livekit::TrackPublishOptions options;
    options.source = livekit::TrackSource::SOURCE_CAMERA;

    // 发布轨道
    m_videoPublication = localParticipant->publishTrack(
        std::static_pointer_cast<livekit::Track>(videoTrack), options);

    m_cameraPublished = true;
    emit cameraPublishedChanged();
    qDebug() << "[LiveKitManager] 摄像头轨道已发布";
  } catch (const std::exception &e) {
    setError(QString("发布摄像头失败: %1").arg(e.what()));
  }
}

void LiveKitManager::unpublishCamera() {
  qDebug() << "[LiveKitManager] 关闭摄像头 (mute), isConnected="
           << m_isConnected << "cameraPublished=" << m_cameraPublished;

  // 【改进】使用 mute 代替 unpublish，避免阻塞
  // unpublishTrack 是阻塞调用，可能导致程序卡死
  if (m_videoPublication) {
    try {
      qDebug() << "[LiveKitManager] 执行视频 mute";
      m_videoPublication->setMuted(true);
      qDebug() << "[LiveKitManager] 摄像头已 mute";
    } catch (const std::exception &e) {
      qWarning() << "[LiveKitManager] mute 摄像头失败:" << e.what();
    }
  }

  // 注意：不再设置 m_cameraPublished = false，因为轨道仍然发布着（只是 muted）
  // 这样下次 publishCamera 时会走 unmute 路径
}

void LiveKitManager::toggleCamera() {
  if (m_cameraPublished) {
    unpublishCamera();
  } else {
    publishCamera();
  }
}

void LiveKitManager::publishMicrophone() {
  qDebug() << "[LiveKitManager] publishMicrophone 被调用, isConnected="
           << m_isConnected << "microphonePublished=" << m_microphonePublished;

  if (!m_isConnected) {
    setError("未连接到房间，无法发布麦克风");
    return;
  }

  // 如果已经发布过，只需要 unmute 和启动本地麦克风
  if (m_microphonePublished && m_audioPublication) {
    qDebug() << "[LiveKitManager] 麦克风已发布，启动本地麦克风并 unmute";

    if (!m_mediaCapture->isMicrophoneActive()) {
      m_mediaCapture->startMicrophone();
    }

    try {
      m_audioPublication->setMuted(false);
      qDebug() << "[LiveKitManager] 麦克风已 unmute";
    } catch (const std::exception &e) {
      qWarning() << "[LiveKitManager] unmute 麦克风失败:" << e.what();
    }
    return;
  }

  // 首次发布：先启动本地麦克风
  if (!m_mediaCapture->isMicrophoneActive()) {
    qDebug() << "[LiveKitManager] 首次发布，启动本地麦克风...";
    m_mediaCapture->startMicrophone();

    QTimer::singleShot(100, this, [this]() { doPublishMicrophoneTrack(); });
    return;
  }

  doPublishMicrophoneTrack();
}

void LiveKitManager::doPublishMicrophoneTrack() {
  qDebug() << "[LiveKitManager] doPublishMicrophoneTrack 被调用";

  if (!m_isConnected || m_microphonePublished) {
    qDebug() << "[LiveKitManager] 跳过发布麦克风 (未连接或已发布)";
    return;
  }

  auto *localParticipant = m_room->localParticipant();
  if (!localParticipant) {
    setError("无法获取本地参会者");
    return;
  }

  try {
    auto audioTrack = m_mediaCapture->getAudioTrack();
    if (!audioTrack) {
      setError("音频轨道未创建");
      return;
    }

    qDebug() << "[LiveKitManager] 正在发布音频轨道...";

    // 设置发布选项
    livekit::TrackPublishOptions options;
    options.source = livekit::TrackSource::SOURCE_MICROPHONE;

    // 发布轨道
    m_audioPublication = localParticipant->publishTrack(
        std::static_pointer_cast<livekit::Track>(audioTrack), options);

    m_microphonePublished = true;
    emit microphonePublishedChanged();
    qDebug() << "[LiveKitManager] 麦克风轨道已发布";
  } catch (const std::exception &e) {
    setError(QString("发布麦克风失败: %1").arg(e.what()));
  }
}

void LiveKitManager::unpublishMicrophone() {
  qDebug() << "[LiveKitManager] 尝试取消发布麦克风, isConnected="
           << m_isConnected << "microphonePublished=" << m_microphonePublished;

  // 【改进】使用 mute 代替 unpublish，避免阻塞
  if (m_audioPublication) {
    try {
      qDebug() << "[LiveKitManager] 执行麦克风 mute";
      m_audioPublication->setMuted(true);
      qDebug() << "[LiveKitManager] 麦克风已 mute";
    } catch (const std::exception &e) {
      qWarning() << "[LiveKitManager] mute 麦克风失败:" << e.what();
    }
  }
}

void LiveKitManager::toggleMicrophone() {
  if (m_microphonePublished) {
    unpublishMicrophone();
  } else {
    publishMicrophone();
  }
}

// ==================== 屏幕共享轨道控制 ====================

void LiveKitManager::publishScreenShare() {
  qDebug() << "[LiveKitManager] publishScreenShare 被调用, isConnected="
           << m_isConnected
           << "screenSharePublished=" << m_screenSharePublished;

  if (!m_isConnected) {
    setError("未连接到房间，无法发布屏幕共享");
    return;
  }

  // 如果已经发布过，只需要 unmute 和启动屏幕捕获
  if (m_screenSharePublished && m_screenSharePublication) {
    qDebug() << "[LiveKitManager] 屏幕共享已发布，启动屏幕捕获并 unmute";

    // 启动屏幕捕获
    if (!m_screenCapture->isActive()) {
      m_screenCapture->startCapture();
    }

    try {
      m_screenSharePublication->setMuted(false);
      qDebug() << "[LiveKitManager] 屏幕共享已 unmute";
    } catch (const std::exception &e) {
      qWarning() << "[LiveKitManager] unmute 屏幕共享失败:" << e.what();
    }
    return;
  }

  // 首次发布：先启动屏幕捕获
  if (!m_screenCapture->isActive()) {
    qDebug() << "[LiveKitManager] 首次发布，启动屏幕捕获...";
    m_screenCapture->startCapture();

    // 等待屏幕捕获启动完成后再发布
    QTimer::singleShot(200, this, [this]() { doPublishScreenShareTrack(); });
    return;
  }

  // 屏幕捕获已经启动，直接发布
  doPublishScreenShareTrack();
}

void LiveKitManager::doPublishScreenShareTrack() {
  qDebug() << "[LiveKitManager] doPublishScreenShareTrack 被调用";

  if (!m_isConnected || m_screenSharePublished) {
    qDebug() << "[LiveKitManager] 跳过发布屏幕共享 (未连接或已发布)";
    return;
  }

  auto *localParticipant = m_room->localParticipant();
  if (!localParticipant) {
    setError("无法获取本地参会者");
    return;
  }

  try {
    auto screenTrack = m_screenCapture->getScreenTrack();
    if (!screenTrack) {
      setError("屏幕共享轨道未创建");
      return;
    }

    qDebug() << "[LiveKitManager] 正在发布屏幕共享轨道...";

    // 设置发布选项 - 使用屏幕共享源类型
    livekit::TrackPublishOptions options;
    options.source = livekit::TrackSource::SOURCE_SCREENSHARE;

    // 发布轨道
    m_screenSharePublication = localParticipant->publishTrack(
        std::static_pointer_cast<livekit::Track>(screenTrack), options);

    m_screenSharePublished = true;
    emit screenSharePublishedChanged();
    qDebug() << "[LiveKitManager] 屏幕共享轨道已发布";
  } catch (const std::exception &e) {
    setError(QString("发布屏幕共享失败: %1").arg(e.what()));
  }
}

void LiveKitManager::unpublishScreenShare() {
  qDebug() << "[LiveKitManager] 取消屏幕共享 (mute), isConnected="
           << m_isConnected
           << "screenSharePublished=" << m_screenSharePublished;

  // 使用 mute 代替 unpublish，避免阻塞
  if (m_screenSharePublication) {
    try {
      qDebug() << "[LiveKitManager] 执行屏幕共享 mute";
      m_screenSharePublication->setMuted(true);
      qDebug() << "[LiveKitManager] 屏幕共享已 mute";
    } catch (const std::exception &e) {
      qWarning() << "[LiveKitManager] mute 屏幕共享失败:" << e.what();
    }
  }

  // 停止屏幕捕获以节省资源
  if (m_screenCapture) {
    m_screenCapture->stopCapture();
  }
}

void LiveKitManager::toggleScreenShare() {
  if (m_screenSharePublished) {
    unpublishScreenShare();
  } else {
    publishScreenShare();
  }
}

void LiveKitManager::setRemoteVideoSink(const QString &participantId,
                                        QVideoSink *sink) {
  qDebug() << "[LiveKitManager] setRemoteVideoSink:" << participantId
           << "sink=" << sink;

  if (m_remoteVideoRenderers.contains(participantId)) {
    // 渲染器已存在，直接绑定
    auto renderer = m_remoteVideoRenderers[participantId];
    if (renderer) {
      renderer->setExternalVideoSink(sink);
    }
  } else {
    // 渲染器还不存在，暂存 sink，等渲染器创建时再绑定
    qDebug() << "[LiveKitManager] 渲染器尚不存在，暂存 sink:" << participantId;
    m_pendingVideoSinks[participantId] = sink;
  }
}
