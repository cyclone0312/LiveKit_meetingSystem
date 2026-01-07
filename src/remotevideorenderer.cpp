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
    qDebug() << "[RemoteVideoRenderer] 渲染循环开始";

    while (m_running.load() && m_videoStream) {
        livekit::VideoFrameEvent event;

        // 阻塞读取视频帧
        bool hasFrame = m_videoStream->read(event);

        if (!hasFrame) {
            // 流已结束或被关闭
            qDebug() << "[RemoteVideoRenderer] 视频流结束";
            break;
        }

        if (!m_running.load()) {
            break;
        }

        // 转换并显示帧
        QVideoFrame qtFrame = convertFrame(event.frame);
        if (qtFrame.isValid()) {
            QMutexLocker locker(&m_mutex);
            if (m_externalSink) {
                m_externalSink->setVideoFrame(qtFrame);
            }
        }
    }

    qDebug() << "[RemoteVideoRenderer] 渲染循环结束";
}

QVideoFrame RemoteVideoRenderer::convertFrame(const livekit::LKVideoFrame &lkFrame)
{
    int width = lkFrame.width();
    int height = lkFrame.height();
    const uint8_t *data = lkFrame.data();
    size_t dataSize = lkFrame.dataSize();

    if (width <= 0 || height <= 0 || !data || dataSize == 0) {
        return QVideoFrame();
    }

    // LiveKit 帧格式是 RGBA
    // 创建 QImage 并复制数据
    QImage image(width, height, QImage::Format_RGBA8888);
    
    // 确保数据大小正确
    size_t expectedSize = static_cast<size_t>(width * height * 4);
    if (dataSize < expectedSize) {
        qWarning() << "[RemoteVideoRenderer] 帧数据大小不匹配:" << dataSize << "vs" << expectedSize;
        return QVideoFrame();
    }

    // 逐行复制（考虑行对齐）
    for (int y = 0; y < height; ++y) {
        memcpy(image.scanLine(y), data + y * width * 4, width * 4);
    }

    // 转换为 QVideoFrame
    QVideoFrame frame(image);
    return frame;
}

void RemoteVideoRenderer::processFrame()
{
    // 此方法保留用于将来可能的信号槽处理模式
}

