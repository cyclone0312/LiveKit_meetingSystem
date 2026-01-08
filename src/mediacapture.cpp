/**
 * @file mediacapture.cpp
 * @brief 媒体采集管理器实现
 */

#include "mediacapture.h"
#include <QDebug>
#include <QMediaFormat>
#include <QVideoFrameFormat>
#include <chrono>


// =============================================================================
// VideoFrameHandler 实现
// =============================================================================

VideoFrameHandler::VideoFrameHandler(QObject *parent) : QObject(parent) {}

void VideoFrameHandler::setVideoSource(
    std::shared_ptr<livekit::VideoSource> source) {
  m_videoSource = source;
  qDebug() << "[VideoFrameHandler] VideoSource 已设置";
}

void VideoFrameHandler::setEnabled(bool enabled) {
  m_enabled = enabled;
  qDebug() << "[VideoFrameHandler] 已" << (enabled ? "启用" : "禁用");
}

void VideoFrameHandler::handleVideoFrame(const QVideoFrame &frame) {
  if (!m_enabled || !m_videoSource) {
    return;
  }

  if (!frame.isValid()) {
    return;
  }

  // 将帧映射为可读模式
  QVideoFrame mappedFrame = frame;
  if (!mappedFrame.map(QVideoFrame::ReadOnly)) {
    qWarning() << "[VideoFrameHandler] 无法映射视频帧";
    return;
  }

  // 获取帧数据
  int width = mappedFrame.width();
  int height = mappedFrame.height();
  QVideoFrameFormat::PixelFormat format = mappedFrame.pixelFormat();

  // 每100帧打印一次调试信息
  if (++m_frameCount % 100 == 0) {
    qDebug() << "[VideoFrameHandler] 处理视频帧 #" << m_frameCount
             << "尺寸:" << width << "x" << height << "格式:" << format;
  }

  // 转换为 ARGB32 格式的 QImage
  QImage image = mappedFrame.toImage();

  if (!image.isNull()) {
    // 转换为 ARGB32 格式
    QImage argbImage = image.convertToFormat(QImage::Format_ARGB32);

    // 创建 LiveKit LKVideoFrame
    // SDK 使用 LKVideoFrame::create() 分配缓冲区
    int frameWidth = argbImage.width();
    int frameHeight = argbImage.height();

    // 创建 BGRA 格式的帧（与 Qt Format_ARGB32 的实际内存布局匹配）
    // 注意：Qt 的 Format_ARGB32 在 Windows 小端系统上内存布局是 B-G-R-A
    livekit::LKVideoFrame lkFrame = livekit::LKVideoFrame::create(
        frameWidth, frameHeight, livekit::VideoBufferType::BGRA);

    // 复制像素数据
    const uchar *srcData = argbImage.constBits();
    uint8_t *dstData = lkFrame.data();
    size_t dataSize = std::min(static_cast<size_t>(argbImage.sizeInBytes()),
                               lkFrame.dataSize());
    std::memcpy(dstData, srcData, dataSize);

    // 获取当前时间戳（微秒）
    auto now = std::chrono::steady_clock::now();
    auto timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                            now.time_since_epoch())
                            .count();

    // 捕获视频帧到 LiveKit
    try {
      m_videoSource->captureFrame(lkFrame, timestamp_us);
    } catch (const std::exception &e) {
      if (m_frameCount % 100 == 0) {
        qWarning() << "[VideoFrameHandler] 捕获帧异常:" << e.what();
      }
    }
  }

  mappedFrame.unmap();
  emit frameProcessed();
}

// =============================================================================
// AudioFrameHandler 实现
// =============================================================================

AudioFrameHandler::AudioFrameHandler(int sampleRate, int channels,
                                     QObject *parent)
    : QIODevice(parent), m_sampleRate(sampleRate), m_numChannels(channels) {
  open(QIODevice::WriteOnly);
}

void AudioFrameHandler::setAudioSource(
    std::shared_ptr<livekit::AudioSource> source) {
  m_audioSource = source;
  qDebug() << "[AudioFrameHandler] AudioSource 已设置";
}

void AudioFrameHandler::setEnabled(bool enabled) {
  m_enabled = enabled;
  qDebug() << "[AudioFrameHandler] 已" << (enabled ? "启用" : "禁用");
}

qint64 AudioFrameHandler::readData(char *data, qint64 maxlen) {
  Q_UNUSED(data);
  Q_UNUSED(maxlen);
  return 0; // 不支持读取
}

qint64 AudioFrameHandler::writeData(const char *data, qint64 len) {
  if (!m_enabled || !m_audioSource || len <= 0) {
    return len; // 静默丢弃
  }

  // 假设使用 16-bit PCM
  int samples_per_channel = len / (2 * m_numChannels);

  if (samples_per_channel <= 0) {
    return len;
  }

  // 构建 int16_t 数据
  std::vector<std::int16_t> audioData(len / 2);
  std::memcpy(audioData.data(), data, len);

  // 创建 LiveKit AudioFrame
  try {
    livekit::AudioFrame audioFrame(std::move(audioData), m_sampleRate,
                                   m_numChannels, samples_per_channel);

    // 发送到 LiveKit（阻塞调用）
    // 【修复】增加超时时间到 100ms，避免频繁超时
    m_audioSource->captureFrame(audioFrame, 100);
  } catch (const std::exception &e) {
    // 静默忽略，避免日志洪泛
  }

  return len;
}

// =============================================================================
// MediaCapture 实现
// =============================================================================

MediaCapture::MediaCapture(QObject *parent)
    : QObject(parent),
      m_captureSession(std::make_unique<QMediaCaptureSession>()),
      m_internalVideoSink(std::make_unique<QVideoSink>()),
      m_videoHandler(std::make_unique<VideoFrameHandler>(this)) {
  qDebug() << "[MediaCapture] 初始化中...";

  // 刷新设备列表
  refreshDevices();

  // 创建 LiveKit 源
  createLiveKitSources();

  // 连接内部视频接收器的信号
  connect(m_internalVideoSink.get(), &QVideoSink::videoFrameChanged, this,
          &MediaCapture::onVideoFrameReceived);

  qDebug() << "[MediaCapture] 初始化完成";
}

MediaCapture::~MediaCapture() {
  stopCamera();
  stopMicrophone();
  qDebug() << "[MediaCapture] 已销毁";
}

void MediaCapture::refreshDevices() {
  // 获取摄像头列表
  m_cameraDevices = QMediaDevices::videoInputs();
  qDebug() << "[MediaCapture] 发现" << m_cameraDevices.count() << "个摄像头:";
  for (const auto &cam : m_cameraDevices) {
    qDebug() << "  -" << cam.description();
  }

  // 获取麦克风列表
  m_microphoneDevices = QMediaDevices::audioInputs();
  qDebug() << "[MediaCapture] 发现" << m_microphoneDevices.count()
           << "个麦克风:";
  for (const auto &mic : m_microphoneDevices) {
    qDebug() << "  -" << mic.description();
  }

  emit availableCamerasChanged();
  emit availableMicrophonesChanged();
}

void MediaCapture::createLiveKitSources() {
  qDebug() << "[MediaCapture] 创建 LiveKit 源...";

  // 创建视频源 - 构造函数接受 (width, height)
  m_lkVideoSource =
      std::make_shared<livekit::VideoSource>(VIDEO_WIDTH, VIDEO_HEIGHT);

  // 创建音频源 - 构造函数接受 (sample_rate, num_channels, queue_size_ms)
  m_lkAudioSource = std::make_shared<livekit::AudioSource>(
      AUDIO_SAMPLE_RATE, AUDIO_CHANNELS, 1000);

  // 设置到处理器
  m_videoHandler->setVideoSource(m_lkVideoSource);

  // 创建音频处理器
  m_audioHandler = std::make_unique<AudioFrameHandler>(AUDIO_SAMPLE_RATE,
                                                       AUDIO_CHANNELS, this);
  m_audioHandler->setAudioSource(m_lkAudioSource);

  // 创建 LiveKit 轨道
  m_lkVideoTrack = livekit::LocalVideoTrack::createLocalVideoTrack(
      "camera", m_lkVideoSource);
  m_lkAudioTrack = livekit::LocalAudioTrack::createLocalAudioTrack(
      "microphone", m_lkAudioSource);

  qDebug() << "[MediaCapture] LiveKit 源和轨道创建完成";
}

// =============================================================================
// 属性 Getter/Setter
// =============================================================================

bool MediaCapture::isCameraActive() const { return m_cameraActive; }

bool MediaCapture::isMicrophoneActive() const { return m_microphoneActive; }

QVideoSink *MediaCapture::videoSink() const { return m_externalVideoSink; }

void MediaCapture::setVideoSink(QVideoSink *sink) {
  qDebug() << "[MediaCapture] setVideoSink 被调用, sink=" << sink
           << "当前 m_externalVideoSink=" << m_externalVideoSink
           << "cameraActive=" << m_cameraActive;

  if (m_externalVideoSink != sink) {
    m_externalVideoSink = sink;
    // 注意：不设置 m_captureSession->setVideoSink(sink)
    // 因为 captureSession 已经连接到内部的 m_internalVideoSink
    // 帧会通过 onVideoFrameReceived 转发到 m_externalVideoSink
    emit videoSinkChanged();
    qDebug() << "[MediaCapture] 外部 VideoSink 已设置:"
             << (sink ? "有效" : "null");
  }
}

QStringList MediaCapture::availableCameras() const {
  QStringList list;
  for (const auto &cam : m_cameraDevices) {
    list.append(cam.description());
  }
  return list;
}

QStringList MediaCapture::availableMicrophones() const {
  QStringList list;
  for (const auto &mic : m_microphoneDevices) {
    list.append(mic.description());
  }
  return list;
}

int MediaCapture::currentCameraIndex() const { return m_currentCameraIndex; }

void MediaCapture::setCurrentCameraIndex(int index) {
  if (index >= 0 && index < m_cameraDevices.count() &&
      m_currentCameraIndex != index) {
    m_currentCameraIndex = index;

    // 如果摄像头正在运行，重新启动使用新设备
    if (m_cameraActive) {
      stopCamera();
      startCamera();
    }

    emit currentCameraIndexChanged();
  }
}

int MediaCapture::currentMicrophoneIndex() const {
  return m_currentMicrophoneIndex;
}

void MediaCapture::setCurrentMicrophoneIndex(int index) {
  if (index >= 0 && index < m_microphoneDevices.count() &&
      m_currentMicrophoneIndex != index) {
    m_currentMicrophoneIndex = index;

    // 如果麦克风正在运行，重新启动使用新设备
    if (m_microphoneActive) {
      stopMicrophone();
      startMicrophone();
    }

    emit currentMicrophoneIndexChanged();
  }
}

// =============================================================================
// 获取 LiveKit 轨道
// =============================================================================

std::shared_ptr<livekit::LocalVideoTrack> MediaCapture::getVideoTrack() {
  return m_lkVideoTrack;
}

std::shared_ptr<livekit::LocalAudioTrack> MediaCapture::getAudioTrack() {
  return m_lkAudioTrack;
}

// =============================================================================
// 重建 LiveKit 轨道（离开房间后调用）
// =============================================================================

void MediaCapture::recreateVideoTrack() {
  if (m_lkVideoSource) {
    m_lkVideoTrack = livekit::LocalVideoTrack::createLocalVideoTrack(
        "camera", m_lkVideoSource);
    qDebug() << "[MediaCapture] 视频轨道已重建";
  }
}

void MediaCapture::recreateAudioTrack() {
  if (m_lkAudioSource) {
    m_lkAudioTrack = livekit::LocalAudioTrack::createLocalAudioTrack(
        "microphone", m_lkAudioSource);
    qDebug() << "[MediaCapture] 音频轨道已重建";
  }
}

// =============================================================================
// 摄像头控制
// =============================================================================

void MediaCapture::startCamera() {
  qDebug() << "[MediaCapture] startCamera 被调用, cameraActive="
           << m_cameraActive;

  if (m_cameraActive) {
    qDebug() << "[MediaCapture] 摄像头已在运行";
    return;
  }

  if (m_cameraDevices.isEmpty()) {
    qWarning() << "[MediaCapture] 没有可用的摄像头";
    emit cameraError("没有可用的摄像头");
    return;
  }

  // 确保旧的摄像头资源已清理
  if (m_camera) {
    qDebug() << "[MediaCapture] 清理旧的摄像头对象...";
    m_captureSession->setCamera(nullptr);
    m_camera.reset();
  }

  qDebug() << "[MediaCapture] 启动摄像头...";

  try {
    // 创建摄像头
    QCameraDevice device = m_cameraDevices.at(m_currentCameraIndex);
    qDebug() << "[MediaCapture] 使用摄像头:" << device.description();

    m_camera = std::make_unique<QCamera>(device);

    // 连接信号
    connect(m_camera.get(), &QCamera::activeChanged, this,
            &MediaCapture::onCameraActiveChanged);
    connect(m_camera.get(), &QCamera::errorOccurred, this,
            &MediaCapture::onCameraErrorOccurred);

    // 设置采集会话
    m_captureSession->setCamera(m_camera.get());

    // 设置视频输出 - 使用内部 sink 来处理帧并转发到 LiveKit
    m_captureSession->setVideoSink(m_internalVideoSink.get());

    // 启动摄像头
    qDebug() << "[MediaCapture] 正在启动摄像头...";
    m_camera->start();

    // 启用视频帧处理
    m_videoHandler->setEnabled(true);

    qDebug() << "[MediaCapture] 摄像头启动命令已发送";
  } catch (const std::exception &e) {
    qCritical() << "[MediaCapture] 启动摄像头异常:" << e.what();
    emit cameraError(QString("启动摄像头失败: %1").arg(e.what()));
  }
}

void MediaCapture::stopCamera() {
  if (!m_cameraActive && !m_camera) {
    return;
  }

  qDebug() << "[MediaCapture] 停止摄像头...";

  // 禁用视频帧处理
  m_videoHandler->setEnabled(false);

  if (m_camera) {
    m_camera->stop();
    m_captureSession->setCamera(nullptr);
    m_camera.reset();
  }

  // 清除外部 VideoSink 引用，防止指向已销毁的 QML 对象
  m_externalVideoSink = nullptr;

  m_cameraActive = false;
  emit cameraActiveChanged();

  qDebug() << "[MediaCapture] 摄像头已停止";
}

void MediaCapture::toggleCamera() {
  if (m_cameraActive) {
    stopCamera();
  } else {
    startCamera();
  }
}

// =============================================================================
// 麦克风控制
// =============================================================================

void MediaCapture::startMicrophone() {
  if (m_microphoneActive) {
    qDebug() << "[MediaCapture] 麦克风已在运行";
    return;
  }

  if (m_microphoneDevices.isEmpty()) {
    qWarning() << "[MediaCapture] 没有可用的麦克风";
    emit microphoneError("没有可用的麦克风");
    return;
  }

  qDebug() << "[MediaCapture] 启动麦克风...";

  // 获取当前选择的设备
  QAudioDevice device = m_microphoneDevices.at(m_currentMicrophoneIndex);

  // 配置音频格式
  QAudioFormat format;
  format.setSampleRate(AUDIO_SAMPLE_RATE);
  format.setChannelCount(AUDIO_CHANNELS);
  format.setSampleFormat(QAudioFormat::Int16);

  // 检查设备是否支持该格式
  if (!device.isFormatSupported(format)) {
    qWarning() << "[MediaCapture] 设备不支持请求的音频格式，使用默认格式";
    format = device.preferredFormat();
  }

  // 创建音频源
  m_audioInput = std::make_unique<QAudioSource>(device, format);

  // 启动音频捕获
  m_audioHandler->setEnabled(true);
  m_audioInput->start(m_audioHandler.get());

  m_microphoneActive = true;
  emit microphoneActiveChanged();

  qDebug() << "[MediaCapture] 麦克风已启动"
           << "采样率:" << format.sampleRate()
           << "声道:" << format.channelCount();
}

void MediaCapture::stopMicrophone() {
  if (!m_microphoneActive) {
    return;
  }

  qDebug() << "[MediaCapture] 停止麦克风...";

  m_audioHandler->setEnabled(false);

  if (m_audioInput) {
    m_audioInput->stop();
    m_audioInput.reset();
  }

  m_microphoneActive = false;
  emit microphoneActiveChanged();

  qDebug() << "[MediaCapture] 麦克风已停止";
}

void MediaCapture::toggleMicrophone() {
  if (m_microphoneActive) {
    stopMicrophone();
  } else {
    startMicrophone();
  }
}

// =============================================================================
// 内部槽函数
// =============================================================================

void MediaCapture::onCameraActiveChanged(bool active) {
  qDebug() << "[MediaCapture] 摄像头活动状态变化:" << active;
  m_cameraActive = active;
  emit cameraActiveChanged();
}

void MediaCapture::onCameraErrorOccurred(QCamera::Error error,
                                         const QString &errorString) {
  qWarning() << "[MediaCapture] 摄像头错误:" << error << errorString;
  emit cameraError(errorString);
}

void MediaCapture::onVideoFrameReceived(const QVideoFrame &frame) {
  // 转发到处理器（处理器会推送到 LiveKit）
  m_videoHandler->handleVideoFrame(frame);

  // 如果有外部 sink，也发送帧
  if (m_externalVideoSink && m_externalVideoSink != m_internalVideoSink.get()) {
    m_externalVideoSink->setVideoFrame(frame);

    // 每100帧打印一次调试信息
    static int frameCount = 0;
    if (++frameCount % 100 == 0) {
      qDebug() << "[MediaCapture] 已发送" << frameCount << "帧到外部 VideoSink";
    }
  }

  emit videoFrameCaptured();
}
