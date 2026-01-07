/**
 * @file livekitmanager.h
 * @brief LiveKit 连接管理器（使用官方 C++ SDK）
 *
 * 负责：
 * 1. 向 Token Server 请求 JWT Token
 * 2. 使用官方 SDK 连接 LiveKit Server
 * 3. 管理房间状态和参会者
 * 4. 处理音视频轨道
 */

#ifndef LIVEKITMANAGER_H
#define LIVEKITMANAGER_H

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <memory>

// LiveKit SDK 头文件
#include <livekit/livekit.h>
#include <livekit/room.h>
#include <livekit/room_delegate.h>
#include <livekit/local_participant.h>
#include <livekit/remote_participant.h>
#include <livekit/local_track_publication.h>

// 媒体采集（需要完整定义，因为 Q_PROPERTY 使用了 MediaCapture*）
#include "mediacapture.h"

// 远程媒体渲染
#include "remotevideorenderer.h"
#include "remoteaudioplayer.h"

#include <QMap>

// 前向声明
class LiveKitManager;

/**
 * @brief LiveKit 房间事件委托
 *
 * 继承自 livekit::RoomDelegate，接收 SDK 的回调事件
 * 并转发给 Qt 的信号槽系统
 */
class LiveKitRoomDelegate : public livekit::RoomDelegate
{
public:
    LiveKitManager *manager = nullptr;

    // 参会者事件
    void onParticipantConnected(livekit::Room &room,
                                const livekit::ParticipantConnectedEvent &event) override;
    void onParticipantDisconnected(livekit::Room &room,
                                   const livekit::ParticipantDisconnectedEvent &event) override;

    // 轨道事件
    void onTrackSubscribed(livekit::Room &room,
                           const livekit::TrackSubscribedEvent &event) override;
    void onTrackUnsubscribed(livekit::Room &room,
                             const livekit::TrackUnsubscribedEvent &event) override;
    void onTrackMuted(livekit::Room &room,
                      const livekit::TrackMutedEvent &event) override;
    void onTrackUnmuted(livekit::Room &room,
                        const livekit::TrackUnmutedEvent &event) override;

    // 连接事件
    void onDisconnected(livekit::Room &room,
                        const livekit::DisconnectedEvent &event) override;
    void onReconnecting(livekit::Room &room,
                        const livekit::ReconnectingEvent &event) override;
    void onReconnected(livekit::Room &room,
                       const livekit::ReconnectedEvent &event) override;

    // 数据事件 - 使用正确的事件类型
    void onUserPacketReceived(livekit::Room &room,
                              const livekit::UserDataPacketEvent &event) override;
};

/**
 * @brief LiveKit 管理器
 *
 * Qt 封装层，提供 QML 可调用的接口
 */
class LiveKitManager : public QObject
{
    Q_OBJECT

    // QML 可访问的属性
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY connectionStateChanged)
    Q_PROPERTY(bool isConnecting READ isConnecting NOTIFY connectionStateChanged)
    Q_PROPERTY(QString currentRoom READ currentRoom NOTIFY currentRoomChanged)
    Q_PROPERTY(QString currentUser READ currentUser NOTIFY currentUserChanged)
    Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(QString tokenServerUrl READ tokenServerUrl WRITE setTokenServerUrl NOTIFY tokenServerUrlChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorOccurred)

    // 媒体状态属性
    Q_PROPERTY(bool cameraPublished READ isCameraPublished NOTIFY cameraPublishedChanged)
    Q_PROPERTY(bool microphonePublished READ isMicrophonePublished NOTIFY microphonePublishedChanged)
    Q_PROPERTY(MediaCapture *mediaCapture READ mediaCapture CONSTANT)

public:
    explicit LiveKitManager(QObject *parent = nullptr);
    ~LiveKitManager();

    // 属性 Getter
    bool isConnected() const;
    bool isConnecting() const;
    QString currentRoom() const;
    QString currentUser() const;
    QString serverUrl() const;
    QString tokenServerUrl() const;
    QString errorMessage() const;

    // 媒体状态 Getter
    bool isCameraPublished() const;
    bool isMicrophonePublished() const;
    MediaCapture *mediaCapture() const;

    // 属性 Setter
    void setServerUrl(const QString &url);
    void setTokenServerUrl(const QString &url);

    // 获取 SDK Room 对象（供高级用途）
    livekit::Room *room() const { return m_room.get(); }

public slots:
    /**
     * @brief 请求 Token 并加入房间
     * @param roomName 房间名称（会议ID）
     * @param userName 用户名称
     */
    void joinRoom(const QString &roomName, const QString &userName);

    /**
     * @brief 离开当前房间
     */
    void leaveRoom();

    /**
     * @brief 设置麦克风静音状态
     */
    void setMicrophoneMuted(bool muted);

    /**
     * @brief 设置摄像头静音状态
     */
    void setCameraMuted(bool muted);

    /**
     * @brief 发送数据消息到房间（用于聊天等）
     * @param data 要发送的数据
     */
    void sendData(const QByteArray &data);

    /**
     * @brief 发布/取消发布摄像头轨道
     */
    void publishCamera();
    void unpublishCamera();
    void toggleCamera();

    /**
     * @brief 发布/取消发布麦克风轨道
     */
    void publishMicrophone();
    void unpublishMicrophone();
    void toggleMicrophone();

    /**
     * @brief 设置远程参会者的视频 Sink（供 QML 调用）
     * @param participantId 参会者 ID
     * @param sink QML VideoOutput 的 videoSink
     */
    Q_INVOKABLE void setRemoteVideoSink(const QString &participantId, QVideoSink *sink);

private:
    /**
     * @brief 实际执行视频轨道发布（内部方法）
     */
    void doPublishCameraTrack();
    void doPublishMicrophoneTrack();

public slots:

    // 兼容旧接口
    void sendChatMessage(const QString &message);
    void updateMicState(bool enabled);
    void updateCameraState(bool enabled);
    void updateScreenShareState(bool enabled);
    void updateHandRaiseState(bool raised);

signals:
    // 连接状态信号
    void connectionStateChanged();
    void connected();
    void disconnected();
    void reconnecting();
    void reconnected();
    void errorOccurred(const QString &error);

    // 房间信息信号
    void currentRoomChanged();
    void currentUserChanged();
    void serverUrlChanged();
    void tokenServerUrlChanged();

    // 参会者信号
    void participantJoined(const QString &identity, const QString &name);
    void participantLeft(const QString &identity);

    // 轨道信号
    void trackSubscribed(const QString &participantIdentity, const QString &trackSid, int trackKind);
    void trackUnsubscribed(const QString &participantIdentity, const QString &trackSid);
    void trackMuted(const QString &participantIdentity, const QString &trackSid, bool muted);

    // 本地媒体发布信号
    void cameraPublishedChanged();
    void microphonePublishedChanged();

    // 兼容旧接口的信号
    void participantMicChanged(const QString &id, bool enabled);
    void participantCameraChanged(const QString &id, bool enabled);
    void participantScreenShareChanged(const QString &id, bool enabled);
    void participantHandRaiseChanged(const QString &id, bool raised);
    void chatMessageReceived(const QString &senderId, const QString &senderName, const QString &message);
    void dataMessageReceived(const QString &type, const QString &data);

    // 数据信号
    void dataReceived(const QString &participantIdentity, const QByteArray &data);

    // Token 信号
    void tokenReceived(const QString &token);
    void tokenRequestFailed(const QString &reason);

    // 远程媒体信号
    void remoteVideoSinkReady(const QString &participantId, QVideoSink *sink);
    void remoteVideoSinkRemoved(const QString &participantId);

private:
    void requestToken(const QString &roomName, const QString &userName);
    void connectToRoom(const QString &token);
    void setError(const QString &error);

private:
    friend class LiveKitRoomDelegate;

    // 网络
    QNetworkAccessManager *m_networkManager;

    // LiveKit SDK
    std::unique_ptr<livekit::Room> m_room;
    std::unique_ptr<LiveKitRoomDelegate> m_delegate;

    // 媒体采集
    std::unique_ptr<MediaCapture> m_mediaCapture;

    // 已发布的轨道
    std::shared_ptr<livekit::LocalTrackPublication> m_videoPublication;
    std::shared_ptr<livekit::LocalTrackPublication> m_audioPublication;
    bool m_cameraPublished = false;
    bool m_microphonePublished = false;

    // 服务器配置
    QString m_serverUrl;
    QString m_tokenServerUrl;

    // 状态
    bool m_isConnected;
    bool m_isConnecting;
    QString m_currentRoom;
    QString m_currentUser;
    QString m_currentToken;
    QString m_errorMessage;

    // 待加入信息
    QString m_pendingRoom;
    QString m_pendingUser;

    // SDK 初始化标志
    static bool s_sdkInitialized;

    // 远程媒体渲染器
    QMap<QString, std::shared_ptr<RemoteVideoRenderer>> m_remoteVideoRenderers;
    QMap<QString, std::shared_ptr<RemoteAudioPlayer>> m_remoteAudioPlayers;
};

#endif // LIVEKITMANAGER_H
