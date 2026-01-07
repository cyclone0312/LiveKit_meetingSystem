/**
 * @file remotevideorenderer.cpp
 * @brief 远程视频渲染器实现
 */

#include "remotevideorenderer.h"
#include <QDebug>
#include <QImage>
#include <thread>

RemoteVideoRenderer::RemoteVideoRenderer(std::shared_ptr<livekit::Track> videoTrack,
                                           QObject *parent)
    : QObject(parent)
    , m_track(videoTrack)
{
    qDebug() << "[RemoteVideoRenderer] 创建远程视频渲染器";
}

RemoteVideoRenderer::~RemoteVideoRenderer()
{
    stop();
    qDebug() << "[RemoteVideoRenderer] 销毁远程视频渲染器";
}

void RemoteVideoRenderer::setExternalVideoSink(QVideoSink *sink)
{
    QMutexLocker locker(&m_mutex);
    m_externalSink = sink;
    qDebug() << "[RemoteVideoRenderer] 设置外部视频 Sink:" << sink;
}

void RemoteVideoRenderer::start()
{
    if (m_running.load()) {
        qDebug() << "[RemoteVideoRenderer] 已在运行中";
        return;
    }

    if (!m_track) {
        emit errorOccurred("视频轨道无效");
        return;
    }

    qDebug() << "[RemoteVideoRenderer] 启动渲染";

    // 创建视频流
    livekit::VideoStream::Options options;
    options.format = livekit::VideoBufferType::RGBA;
    options.capacity = 3;  // 缓冲 3 帧

    try {
        m_videoStream = livekit::VideoStream::fromTrack(m_track, options);
        if (!m_videoStream) {
            emit errorOccurred("无法创建视频流");
            return;
        }
    } catch (const std::exception &e) {
        emit errorOccurred(QString("创建视频流失败: %1").arg(e.what()));
        return;
    }

    m_running.store(true);

    // 在后台线程中运行渲染循环
    std::thread([this]() {
        renderLoop();
    }).detach();
}

void RemoteVideoRenderer::stop()
{
    if (!m_running.load()) {
        return;
    }

    qDebug() << "[RemoteVideoRenderer] 停止渲染";
    m_running.store(false);

    // 关闭视频流（这会唤醒阻塞的 read() 调用）
    if (m_videoStream) {
        m_videoStream->close();
        m_videoStream.reset();
    }
}

void RemoteVideoRenderer::renderLoop()
{
    qDebug() << "[RemoteVideoRenderer] 渲染循环开始, participantId=" << m_participantId;

    int frameCount = 0;
    
    while (m_running.load() && m_videoStream) {
        livekit::VideoFrameEvent event;

        // 阻塞读取视频帧
        bool hasFrame = m_videoStream->read(event);

        if (!hasFrame) {
            // 流已结束或被关闭
            qDebug() << "[RemoteVideoRenderer] 视频流结束, participantId=" << m_participantId;
            break;
        }

        if (!m_running.load()) {
            break;
        }

        frameCount++;
        
        // 每 100 帧打印一次日志
        if (frameCount % 100 == 1) {
            qDebug() << "[RemoteVideoRenderer] 收到远程帧 #" << frameCount 
                     << "participantId=" << m_participantId
                     << "尺寸=" << event.frame.width() << "x" << event.frame.height()
                     << "sink=" << (m_externalSink ? "已设置" : "未设置");
        }

        // 转换并显示帧
        QVideoFrame qtFrame = convertFrame(event.frame);
        if (qtFrame.isValid()) {
            QMutexLocker locker(&m_mutex);
            if (m_externalSink) {
                m_externalSink->setVideoFrame(qtFrame);
            } else if (frameCount % 100 == 1) {
                qWarning() << "[RemoteVideoRenderer] 没有 sink，无法显示帧! participantId=" << m_participantId;
            }
        } else if (frameCount % 100 == 1) {
            qWarning() << "[RemoteVideoRenderer] 帧转换失败! participantId=" << m_participantId;
        }
    }

    qDebug() << "[RemoteVideoRenderer] 渲染循环结束, 共处理" << frameCount << "帧, participantId=" << m_participantId;
}

QVideoFrame RemoteVideoRenderer::convertFrame(const livekit::LKVideoFrame &lkFrame)
{
    int width = lkFrame.width();
    int height = lkFrame.height();

    if (width <= 0 || height <= 0) {
        return QVideoFrame();
    }

    // 使用 SDK 的 convert 函数将 RGBA 转换为 BGRA（Qt 在 Windows 上的原生格式）
    livekit::LKVideoFrame bgraFrame = lkFrame.convert(livekit::VideoBufferType::BGRA, false);
    
    const uint8_t *data = bgraFrame.data();
    size_t dataSize = bgraFrame.dataSize();
    
    if (!data || dataSize == 0) {
        return QVideoFrame();
    }

    // BGRA 对应 Qt 的 Format_ARGB32（Windows 小端字节序）
    QImage image(width, height, QImage::Format_ARGB32);
    
    // 直接 memcpy（BGRA 和 ARGB32 在内存中布局相同）
    size_t bytesPerLine = width * 4;
    for (int y = 0; y < height; ++y) {
        memcpy(image.scanLine(y), data + y * bytesPerLine, bytesPerLine);
    }

    // 转换为 QVideoFrame
    QVideoFrame frame(image);
    return frame;
}

void RemoteVideoRenderer::processFrame()
{
    // 此方法保留用于将来可能的信号槽处理模式
}

