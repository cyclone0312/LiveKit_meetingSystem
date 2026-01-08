/**
 * @file remotevideorenderer.cpp
 * @brief 远程视频渲染器实现
 */

#include "remotevideorenderer.h"
#include <QDebug>
#include <QImage>
#include <QVideoFrameFormat>
#include <thread>

RemoteVideoRenderer::RemoteVideoRenderer(
    std::shared_ptr<livekit::Track> videoTrack, QObject *parent)
    : QObject(parent), m_track(videoTrack) {
  qDebug() << "[RemoteVideoRenderer] 创建远程视频渲染器";
}

RemoteVideoRenderer::~RemoteVideoRenderer() {
  stop();
  qDebug() << "[RemoteVideoRenderer] 销毁远程视频渲染器";
}

void RemoteVideoRenderer::setExternalVideoSink(QVideoSink *sink) {
  QMutexLocker locker(&m_mutex);
  m_externalSink = sink;
  qDebug() << "[RemoteVideoRenderer] 设置外部视频 Sink:" << sink;
}

void RemoteVideoRenderer::start() {
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
  // 使用 BGRA 格式，与发送端保持一致
  options.format = livekit::VideoBufferType::BGRA;
  options.capacity = 3; // 缓冲 3 帧

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

  // 在后台线程中运行渲染循环（保存线程引用以便 stop 时 join）
  m_renderThread = std::thread([this]() { renderLoop(); });
}

void RemoteVideoRenderer::stop() {
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

  // 等待渲染线程结束
  if (m_renderThread.joinable()) {
    m_renderThread.join();
  }
}

void RemoteVideoRenderer::renderLoop() {
  qDebug() << "[RemoteVideoRenderer] 渲染循环开始, participantId="
           << m_participantId;

  int frameCount = 0;

  while (m_running.load() && m_videoStream) {
    livekit::VideoFrameEvent event;

    // 阻塞读取视频帧
    bool hasFrame = m_videoStream->read(event);

    if (!hasFrame) {
      // 流已结束或被关闭
      qDebug() << "[RemoteVideoRenderer] 视频流结束, participantId="
               << m_participantId;
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
      // 【关键修复】将 QPointer 复制到局部变量
      // 使用 QPointer 确保在 lambda 执行时能检测到对象是否已销毁
      QPointer<QVideoSink> sinkPointer = m_externalSink;
      if (sinkPointer) {
        // 【关键修复】使用 QMetaObject::invokeMethod 确保在主线程设置视频帧
        // lambda 捕获 QPointer，在执行时会自动检测对象是否仍然有效
        QMetaObject::invokeMethod(
            sinkPointer.data(),
            [sinkPointer, qtFrame]() {
              // 再次检查 QPointer，因为在排队期间对象可能被销毁
              if (sinkPointer) {
                sinkPointer->setVideoFrame(qtFrame);
              }
            },
            Qt::QueuedConnection);
      } else if (frameCount % 100 == 1) {
        qWarning()
            << "[RemoteVideoRenderer] 没有 sink，无法显示帧! participantId="
            << m_participantId;
      }
    } else if (frameCount % 100 == 1) {
      qWarning() << "[RemoteVideoRenderer] 帧转换失败! participantId="
                 << m_participantId;
    }
  }

  qDebug() << "[RemoteVideoRenderer] 渲染循环结束, 共处理" << frameCount
           << "帧, participantId=" << m_participantId;
}

QVideoFrame
RemoteVideoRenderer::convertFrame(const livekit::LKVideoFrame &lkFrame) {
  int width = lkFrame.width();
  int height = lkFrame.height();

  if (width <= 0 || height <= 0) {
    return QVideoFrame();
  }

  // 获取 ARGB 数据（与发送端格式一致）
  const uint8_t *data = lkFrame.data();
  size_t dataSize = lkFrame.dataSize();

  if (!data || dataSize == 0) {
    return QVideoFrame();
  }

  // SDK 返回 BGRA 格式，与 Qt 的 Format_BGRA8888 完全匹配
  // 直接复制数据即可，无需逐像素转换
  QVideoFrameFormat format(QSize(width, height),
                           QVideoFrameFormat::Format_BGRA8888);
  QVideoFrame frame(format);

  if (!frame.map(QVideoFrame::WriteOnly)) {
    return QVideoFrame();
  }

  // 直接复制 BGRA 数据
  uint8_t *destData = frame.bits(0);
  int destBytesPerLine = frame.bytesPerLine(0);
  size_t srcBytesPerLine = width * 4;

  // 按行复制（考虑可能的行对齐差异）
  for (int y = 0; y < height; ++y) {
    const uint8_t *srcRow = data + y * srcBytesPerLine;
    uint8_t *destRow = destData + y * destBytesPerLine;
    std::memcpy(destRow, srcRow, srcBytesPerLine);
  }

  frame.unmap();
  return frame;
}

void RemoteVideoRenderer::processFrame() {
  // 此方法保留用于将来可能的信号槽处理模式
}
