/**
 * @file remotevideorenderer.h
 * @brief 远程视频渲染器
 *
 * 负责从 LiveKit 远程视频轨道读取视频帧，
 * 转换为 Qt 格式并发送到外部 QVideoSink 供 QML 显示。
 */

#ifndef REMOTEVIDEORENDERER_H
#define REMOTEVIDEORENDERER_H

#include <QObject>
#include <QVideoSink>
#include <QVideoFrame>
#include <QThread>
#include <QMutex>
#include <memory>
#include <atomic>

// LiveKit SDK
#include <livekit/video_stream.h>
#include <livekit/video_frame.h>
#include <livekit/track.h>

/**
 * @brief 远程视频渲染器
 *
 * 从 LiveKit 远程视频轨道读取帧并渲染到外部 QVideoSink
 */
class RemoteVideoRenderer : public QObject
{
    Q_OBJECT

public:
    explicit RemoteVideoRenderer(std::shared_ptr<livekit::Track> videoTrack,
                                  QObject *parent = nullptr);
    ~RemoteVideoRenderer();

    /**
     * @brief 设置外部视频 Sink（来自 QML VideoOutput）
     * 渲染器会将帧写入此 Sink
     */
    Q_INVOKABLE void setExternalVideoSink(QVideoSink *sink);

    /**
     * @brief 获取关联的参会者 ID
     */
    QString participantId() const { return m_participantId; }
    void setParticipantId(const QString &id) { m_participantId = id; }

    /**
     * @brief 启动渲染
     */
    void start();

    /**
     * @brief 停止渲染
     */
    void stop();

    /**
     * @brief 是否正在运行
     */
    bool isRunning() const { return m_running.load(); }

signals:
    void frameReady();
    void errorOccurred(const QString &error);

private slots:
    void processFrame();

private:
    /**
     * @brief 将 LiveKit 视频帧转换为 Qt 视频帧
     */
    QVideoFrame convertFrame(const livekit::LKVideoFrame &lkFrame);

    /**
     * @brief 渲染循环（在工作线程中运行）
     */
    void renderLoop();

private:
    // LiveKit 组件
    std::shared_ptr<livekit::Track> m_track;
    std::shared_ptr<livekit::VideoStream> m_videoStream;

    // 外部视频 Sink（来自 QML VideoOutput）
    QVideoSink *m_externalSink{nullptr};

    // 状态
    std::atomic<bool> m_running{false};
    QString m_participantId;

    // 互斥锁
    QMutex m_mutex;
};

#endif // REMOTEVIDEORENDERER_H
