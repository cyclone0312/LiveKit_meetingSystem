/**
 * @file mediacapture.cpp
 * @brief 媒体采集管理器实现
 */

#include "mediacapture.h"
#include "remoteaudioplayer.h"
#include <QDebug>
#include <QMediaFormat>
#include <QThread>
#include <QVideoFrameFormat>
#include <chrono>

// =============================================================================
// VideoFrameHandler 实现
// =============================================================================

VideoFrameHandler::VideoFrameHandler(QObject *parent) : QObject(parent) {}

void VideoFrameHandler::setVideoSource(
    std::shared_ptr<livekit::VideoSource> source)
{
  m_videoSource = source;
  qDebug() << "[VideoFrameHandler] VideoSource 已设置";
}

void VideoFrameHandler::setEnabled(bool enabled)
{
  m_enabled = enabled;
  qDebug() << "[VideoFrameHandler] 已" << (enabled ? "启用" : "禁用");
}

void VideoFrameHandler::handleVideoFrame(const QVideoFrame &frame)
{
  if (!m_enabled || !m_videoSource)
  {
    return;
  }

  if (!frame.isValid())
  {
    return;
  }

  // 【优化】帧率控制：限制发送到 LiveKit 的帧率不超过 25fps
  // 避免发送端占用过多带宽和 CPU
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                     now - m_lastFrameTime)
                     .count();
  if (elapsed < 40000) // 40ms = 25fps
  {
    return; // 跳过这一帧
  }
  m_lastFrameTime = now;

  // 将帧映射为可读模式
  QVideoFrame mappedFrame = frame;
  if (!mappedFrame.map(QVideoFrame::ReadOnly))
  {
    qWarning() << "[VideoFrameHandler] 无法映射视频帧";
    return;
  }

  // 获取帧数据
  int width = mappedFrame.width();
  int height = mappedFrame.height();
  QVideoFrameFormat::PixelFormat format = mappedFrame.pixelFormat();

  // 每100帧打印一次调试信息
  if (++m_frameCount % 100 == 0)
  {
    qDebug() << "[VideoFrameHandler] 处理视频帧 #" << m_frameCount
             << "尺寸:" << width << "x" << height << "格式:" << format;
  }

  // 【优化】尝试直接从 QVideoFrame 获取 BGRA/ARGB 数据，避免多次拷贝
  bool directCopy = false;
  if (format == QVideoFrameFormat::Format_BGRA8888 ||
      format == QVideoFrameFormat::Format_ARGB8888 ||
      format == QVideoFrameFormat::Format_BGRX8888 ||
      format == QVideoFrameFormat::Format_RGBX8888)
  {
    // 这些格式的内存布局与 BGRA 兼容（在 Windows 小端系统上）
    // 可以直接从映射内存中复制数据到 LiveKit 帧
    const uint8_t *srcData = mappedFrame.bits(0);
    int srcBytesPerLine = mappedFrame.bytesPerLine(0);
    size_t expectedBytesPerLine = width * 4;

    if (srcData && srcBytesPerLine >= static_cast<int>(expectedBytesPerLine))
    {
      livekit::VideoFrame lkFrame = livekit::VideoFrame::create(
          width, height, livekit::VideoBufferType::BGRA);

      uint8_t *dstData = lkFrame.data();

      // 如果行对齐的，单次 memcpy
      if (static_cast<size_t>(srcBytesPerLine) == expectedBytesPerLine)
      {
        std::memcpy(dstData, srcData, expectedBytesPerLine * height);
      }
      else
      {
        for (int y = 0; y < height; ++y)
        {
          std::memcpy(dstData + y * expectedBytesPerLine,
                      srcData + y * srcBytesPerLine, expectedBytesPerLine);
        }
      }

      auto timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                              now.time_since_epoch())
                              .count();

      try
      {
        m_videoSource->captureFrame(lkFrame, timestamp_us);
      }
      catch (const std::exception &e)
      {
        if (m_frameCount % 100 == 0)
        {
          qWarning() << "[VideoFrameHandler] 捕获帧异常:" << e.what();
        }
      }
      directCopy = true;
    }
  }

  // 【回退路径】如果无法直接复制，使用 toImage 转换（原始方法）
  if (!directCopy)
  {
    QImage image = mappedFrame.toImage();
    if (!image.isNull())
    {
      QImage argbImage = image.convertToFormat(QImage::Format_ARGB32);

      int frameWidth = argbImage.width();
      int frameHeight = argbImage.height();

      livekit::VideoFrame lkFrame = livekit::VideoFrame::create(
          frameWidth, frameHeight, livekit::VideoBufferType::BGRA);

      const uchar *srcImgData = argbImage.constBits();
      uint8_t *dstFrmData = lkFrame.data();
      size_t dataSize = std::min(static_cast<size_t>(argbImage.sizeInBytes()),
                                 lkFrame.dataSize());
      std::memcpy(dstFrmData, srcImgData, dataSize);

      auto timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                              now.time_since_epoch())
                              .count();

      try
      {
        m_videoSource->captureFrame(lkFrame, timestamp_us);
      }
      catch (const std::exception &e)
      {
        if (m_frameCount % 100 == 0)
        {
          qWarning() << "[VideoFrameHandler] 捕获帧异常:" << e.what();
        }
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
    : QIODevice(parent), m_sampleRate(sampleRate), m_numChannels(channels),
      m_samplesPerFrame10ms(sampleRate / 100)
{
  open(QIODevice::WriteOnly);
  // 预分配缓冲区容量（10ms × channels × 2帧余量）
  m_frameBuffer.reserve(m_samplesPerFrame10ms * channels * 2);
  qDebug() << "[AudioFrameHandler] 10ms 帧大小:" << m_samplesPerFrame10ms
           << "采样/声道 (采样率=" << sampleRate << "声道=" << channels << ")";
}

void AudioFrameHandler::setAudioSource(
    std::shared_ptr<livekit::AudioSource> source)
{
  m_audioSource = source;
  qDebug() << "[AudioFrameHandler] AudioSource 已设置";
}

void AudioFrameHandler::setEnabled(bool enabled)
{
  m_enabled = enabled;
  qDebug() << "[AudioFrameHandler] 已" << (enabled ? "启用" : "禁用");
}

qint64 AudioFrameHandler::readData(char *data, qint64 maxlen)
{
  Q_UNUSED(data);
  Q_UNUSED(maxlen);
  return 0; // 不支持读取
}

qint64 AudioFrameHandler::writeData(const char *data, qint64 len)
{
  // 【关键修复原理：优雅退出】
  // 检查停止标志，主线程在销毁资源前会设置 m_stopping=true
  if (!m_enabled || !m_audioSource || m_stopping.load() || len <= 0)
  {
    return len; // 静默丢弃
  }

  // 发出原始 PCM 数据信号，供 AI 语音转录使用
  emit rawAudioCaptured(QByteArray(data, static_cast<int>(len)), m_sampleRate,
                        m_numChannels);

  // 假设使用 16-bit PCM
  int totalSamples = static_cast<int>(len / sizeof(std::int16_t));
  if (totalSamples <= 0)
  {
    return len;
  }

  // 将新数据追加到帧缓冲区
  const auto *samples = reinterpret_cast<const std::int16_t *>(data);
  m_frameBuffer.insert(m_frameBuffer.end(), samples, samples + totalSamples);

  // 处理并发送积攒的 10ms 帧
  processAndSendFrames();

  return len;
}

void AudioFrameHandler::processAndSendFrames()
{
  // 每帧总采样数 = 每声道采样数 × 声道数
  const int frameTotalSamples = m_samplesPerFrame10ms * m_numChannels;

  // 循环处理所有完整的 10ms 帧
  while (static_cast<int>(m_frameBuffer.size()) >= frameTotalSamples)
  {
    // 提取一帧
    std::vector<std::int16_t> frameData(
        m_frameBuffer.begin(), m_frameBuffer.begin() + frameTotalSamples);
    m_frameBuffer.erase(m_frameBuffer.begin(),
                        m_frameBuffer.begin() + frameTotalSamples);

    // 创建 AudioFrame
    livekit::AudioFrame audioFrame(std::move(frameData), m_sampleRate,
                                   m_numChannels, m_samplesPerFrame10ms);

    // 【关键】通过 APM 处理音频（降噪、回声消除、AGC、高通滤波）
    if (m_apm)
    {
      try
      {
        m_apm->processStream(audioFrame);
      }
      catch (const std::exception &e)
      {
        // 忽略 APM 处理失败，仍然发送原始音频
      }
    }

    // 【关键修复】同步发送，queue_size_ms=0 时立即返回，不会阻塞
    try
    {
      if (m_audioSource && !m_stopping.load())
      {
        m_audioSource->captureFrame(audioFrame);
      }
    }
    catch (const std::exception &e)
    {
      // 静默忽略，避免日志洪泛
    }
  }
}

// =============================================================================
// MediaCapture 实现
// =============================================================================

MediaCapture::MediaCapture(QObject *parent)
    : QObject(parent),
      m_captureSession(std::make_unique<QMediaCaptureSession>()),
      m_internalVideoSink(std::make_unique<QVideoSink>()),
      m_videoHandler(std::make_unique<VideoFrameHandler>(this))
{
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

MediaCapture::~MediaCapture()
{
  stopCamera();
  stopMicrophone();
  qDebug() << "[MediaCapture] 已销毁";
}

void MediaCapture::refreshDevices()
{
  // 获取摄像头列表
  m_cameraDevices = QMediaDevices::videoInputs();
  qDebug() << "[MediaCapture] 发现" << m_cameraDevices.count() << "个摄像头:";
  for (const auto &cam : m_cameraDevices)
  {
    qDebug() << "  -" << cam.description();
  }

  // 获取麦克风列表
  m_microphoneDevices = QMediaDevices::audioInputs();
  qDebug() << "[MediaCapture] 发现" << m_microphoneDevices.count()
           << "个麦克风:";
  for (const auto &mic : m_microphoneDevices)
  {
    qDebug() << "  -" << mic.description();
  }

  emit availableCamerasChanged();
  emit availableMicrophonesChanged();
}

void MediaCapture::createLiveKitSources()
{
  qDebug() << "[MediaCapture] 创建 LiveKit 源...";

  // 创建视频源 - 构造函数接受 (width, height)
  m_lkVideoSource =
      std::make_shared<livekit::VideoSource>(VIDEO_WIDTH, VIDEO_HEIGHT);

  // 创建音频源 - queue_size_ms=0 为实时采集模式，最低延迟
  m_lkAudioSource = std::make_shared<livekit::AudioSource>(AUDIO_SAMPLE_RATE,
                                                           AUDIO_CHANNELS, 0);

  // 【关键】创建音频处理模块（回声消除 + 降噪 + AGC + 高通滤波）
  livekit::AudioProcessingModule::Options apmOpts;
  apmOpts.echo_cancellation = true;
  apmOpts.noise_suppression = true;
  apmOpts.auto_gain_control = true;
  apmOpts.high_pass_filter = true;
  try
  {
    m_apm = std::make_unique<livekit::AudioProcessingModule>(apmOpts);
    qDebug() << "[MediaCapture] APM 创建成功 (回声消除+降噪+AGC+高通滤波)";
    // 共享 APM 给 RemoteAudioPlayer，用于回声消除参考信号
    RemoteAudioPlayer::setSharedAPM(m_apm.get());
  }
  catch (const std::exception &e)
  {
    qWarning() << "[MediaCapture] APM 创建失败:" << e.what();
    m_apm.reset();
  }

  // 设置到处理器
  m_videoHandler->setVideoSource(m_lkVideoSource);

  // 创建音频处理器
  m_audioHandler = std::make_unique<AudioFrameHandler>(AUDIO_SAMPLE_RATE,
                                                       AUDIO_CHANNELS, this);
  m_audioHandler->setAudioSource(m_lkAudioSource);
  if (m_apm)
  {
    m_audioHandler->setAPM(m_apm.get());
  }
  // 转发原始 PCM 数据信号（供 AI 语音转录）
  connect(m_audioHandler.get(), &AudioFrameHandler::rawAudioCaptured, this,
          &MediaCapture::rawAudioCaptured);

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

void MediaCapture::setVideoSink(QVideoSink *sink)
{
  qDebug() << "[MediaCapture] setVideoSink 被调用, sink=" << sink
           << "当前 m_externalVideoSink=" << m_externalVideoSink
           << "cameraActive=" << m_cameraActive;

  if (m_externalVideoSink != sink)
  {
    m_externalVideoSink = sink;
    // 注意：不设置 m_captureSession->setVideoSink(sink)
    // 因为 captureSession 已经连接到内部的 m_internalVideoSink
    // 帧会通过 onVideoFrameReceived 转发到 m_externalVideoSink
    emit videoSinkChanged();
    qDebug() << "[MediaCapture] 外部 VideoSink 已设置:"
             << (sink ? "有效" : "null");
  }
}

QStringList MediaCapture::availableCameras() const
{
  QStringList list;
  for (const auto &cam : m_cameraDevices)
  {
    list.append(cam.description());
  }
  return list;
}

QStringList MediaCapture::availableMicrophones() const
{
  QStringList list;
  for (const auto &mic : m_microphoneDevices)
  {
    list.append(mic.description());
  }
  return list;
}

int MediaCapture::currentCameraIndex() const { return m_currentCameraIndex; }

void MediaCapture::setCurrentCameraIndex(int index)
{
  if (index >= 0 && index < m_cameraDevices.count() &&
      m_currentCameraIndex != index)
  {
    m_currentCameraIndex = index;

    // 如果摄像头正在运行，重新启动使用新设备
    if (m_cameraActive)
    {
      stopCamera();
      startCamera();
    }

    emit currentCameraIndexChanged();
  }
}

int MediaCapture::currentMicrophoneIndex() const
{
  return m_currentMicrophoneIndex;
}

void MediaCapture::setCurrentMicrophoneIndex(int index)
{
  if (index >= 0 && index < m_microphoneDevices.count() &&
      m_currentMicrophoneIndex != index)
  {
    m_currentMicrophoneIndex = index;

    // 如果麦克风正在运行，重新启动使用新设备
    if (m_microphoneActive)
    {
      stopMicrophone();
      startMicrophone();
    }

    emit currentMicrophoneIndexChanged();
  }
}

// =============================================================================
// 获取 LiveKit 轨道
// =============================================================================

std::shared_ptr<livekit::LocalVideoTrack> MediaCapture::getVideoTrack()
{
  return m_lkVideoTrack;
}

std::shared_ptr<livekit::LocalAudioTrack> MediaCapture::getAudioTrack()
{
  return m_lkAudioTrack;
}

// =============================================================================
// 重建 LiveKit 轨道（离开房间后调用）
// =============================================================================

void MediaCapture::recreateVideoTrack()
{
  if (m_lkVideoSource)
  {
    m_lkVideoTrack = livekit::LocalVideoTrack::createLocalVideoTrack(
        "camera", m_lkVideoSource);
    qDebug() << "[MediaCapture] 视频轨道已重建";
  }
}

void MediaCapture::recreateAudioTrack()
{
  if (m_lkAudioSource)
  {
    m_lkAudioTrack = livekit::LocalAudioTrack::createLocalAudioTrack(
        "microphone", m_lkAudioSource);
    qDebug() << "[MediaCapture] 音频轨道已重建";
  }
}

void MediaCapture::resetLiveKitSources()
{
  qDebug() << "[MediaCapture] 重置所有 LiveKit 源和轨道...";

  // 【关键修复原理：优雅退出 - 第二步】
  // 亮红灯：通知所有正在运行的后台线程立即停止。
  // 设置原子标志位，确保多线程可见性。
  if (m_audioHandler)
  {
    m_audioHandler->setStopping(true);
    m_audioHandler->setEnabled(false);
  }
  if (m_videoHandler)
  {
    m_videoHandler->setEnabled(false);
  }

  // 清空旧的轨道和源（释放 shared_ptr 引用）
  // 这会等待后台线程完成当前操作（因为我们已经设置了 100ms 超时）
  m_lkVideoTrack.reset();
  m_lkAudioTrack.reset();
  m_lkVideoSource.reset();
  m_lkAudioSource.reset();

  // 给后台线程足够的时间来完成当前操作
  QThread::msleep(100);

  // 创建全新的 LiveKit 源
  m_lkVideoSource =
      std::make_shared<livekit::VideoSource>(VIDEO_WIDTH, VIDEO_HEIGHT);
  m_lkAudioSource = std::make_shared<livekit::AudioSource>(
      AUDIO_SAMPLE_RATE, AUDIO_CHANNELS, 0); // 实时模式

  // 更新处理器中的源引用
  m_videoHandler->setVideoSource(m_lkVideoSource);

  // 重新创建音频处理器（确保它也使用新的 AudioSource）
  m_audioHandler = std::make_unique<AudioFrameHandler>(AUDIO_SAMPLE_RATE,
                                                       AUDIO_CHANNELS, this);
  m_audioHandler->setAudioSource(m_lkAudioSource);
  if (m_apm)
  {
    m_audioHandler->setAPM(m_apm.get());
  }
  // 【关键】重置停止标志，允许新的后台线程工作
  m_audioHandler->setStopping(false);
  // 转发原始 PCM 数据信号（供 AI 语音转录）
  connect(m_audioHandler.get(), &AudioFrameHandler::rawAudioCaptured, this,
          &MediaCapture::rawAudioCaptured);

  // 创建新的轨道
  m_lkVideoTrack = livekit::LocalVideoTrack::createLocalVideoTrack(
      "camera", m_lkVideoSource);
  m_lkAudioTrack = livekit::LocalAudioTrack::createLocalAudioTrack(
      "microphone", m_lkAudioSource);

  qDebug() << "[MediaCapture] LiveKit 源和轨道重置完成";
}

// =============================================================================
// 摄像头控制
// =============================================================================

void MediaCapture::startCamera()
{
  qDebug() << "[MediaCapture] startCamera 被调用, cameraActive="
           << m_cameraActive;

  if (m_cameraActive)
  {
    qDebug() << "[MediaCapture] 摄像头已在运行";
    return;
  }

  if (m_cameraDevices.isEmpty())
  {
    qWarning() << "[MediaCapture] 没有可用的摄像头";
    emit cameraError("没有可用的摄像头");
    return;
  }

  // 确保旧的摄像头资源已清理
  if (m_camera)
  {
    qDebug() << "[MediaCapture] 清理旧的摄像头对象...";
    m_captureSession->setCamera(nullptr);
    m_camera.reset();
  }

  qDebug() << "[MediaCapture] 启动摄像头...";

  try
  {
    // 创建摄像头
    QCameraDevice device = m_cameraDevices.at(m_currentCameraIndex);
    qDebug() << "[MediaCapture] 使用摄像头:" << device.description();

    m_camera = std::make_unique<QCamera>(device);

    // 【优化】设置摄像头分辨率约束，确保以最佳 480p 格式采集
    QCameraFormat bestFormat;
    int bestScore = -1;
    for (const auto &fmt : device.videoFormats())
    {
      QSize res = fmt.resolution();
      // 优先精确匹配 640x480，帧率 >= 25fps
      if (res.width() == 640 && res.height() == 480 &&
          fmt.maxFrameRate() >= 25.0)
      {
        bestFormat = fmt;
        bestScore = 1000; // 精确匹配最高优先级
        break;
      }
      // 次优：选择最接近 640x480 且帧率 >= 20fps 的格式
      if (fmt.maxFrameRate() >= 20.0)
      {
        int score = 0;
        // 像素数越接近 640*480 = 307200 越好
        int pixels = res.width() * res.height();
        int diff = std::abs(pixels - 307200);
        score = 500000 - diff; // 差距越小分数越高
        if (score > bestScore)
        {
          bestScore = score;
          bestFormat = fmt;
        }
      }
    }
    if (!bestFormat.isNull())
    {
      m_camera->setCameraFormat(bestFormat);
      qDebug() << "[MediaCapture] 摄像头格式已设置:"
               << bestFormat.resolution().width() << "x"
               << bestFormat.resolution().height()
               << "@" << bestFormat.maxFrameRate() << "fps";
    }
    else
    {
      qWarning() << "[MediaCapture] 未找到合适的摄像头格式，使用默认值";
    }

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
  }
  catch (const std::exception &e)
  {
    qCritical() << "[MediaCapture] 启动摄像头异常:" << e.what();
    emit cameraError(QString("启动摄像头失败: %1").arg(e.what()));
  }
}

void MediaCapture::stopCamera()
{
  if (!m_cameraActive && !m_camera)
  {
    return;
  }

  qDebug() << "[MediaCapture] 停止摄像头...";

  // 禁用视频帧处理
  m_videoHandler->setEnabled(false);

  if (m_camera)
  {
    m_camera->stop();
    m_captureSession->setCamera(nullptr);
    m_camera.reset();
  }

  // 注意：不再清除 m_externalVideoSink
  // QML 负责管理 VideoSink 的生命周期，当 VideoOutput 销毁时会设置为 null
  // 这样避免重新加入房间时因 VideoSink 被清空而导致崩溃

  m_cameraActive = false;
  emit cameraActiveChanged();

  qDebug() << "[MediaCapture] 摄像头已停止";
}

void MediaCapture::toggleCamera()
{
  if (m_cameraActive)
  {
    stopCamera();
  }
  else
  {
    startCamera();
  }
}

// =============================================================================
// 麦克风控制
// =============================================================================

void MediaCapture::startMicrophone()
{
  if (m_microphoneActive)
  {
    qDebug() << "[MediaCapture] 麦克风已在运行";
    return;
  }

  if (m_microphoneDevices.isEmpty())
  {
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
  bool formatChanged = false;
  if (!device.isFormatSupported(format))
  {
    qWarning() << "[MediaCapture] 设备不支持请求的音频格式，使用默认格式";
    format = device.preferredFormat();
    // 确保采样格式仍然是 Int16
    format.setSampleFormat(QAudioFormat::Int16);
    formatChanged = true;
  }

  // 【关键修复】如果实际格式与配置不同，需要重新创建 AudioSource 和
  // AudioFrameHandler
  int actualSampleRate = format.sampleRate();
  int actualChannels = format.channelCount();

  if (formatChanged || actualSampleRate != AUDIO_SAMPLE_RATE ||
      actualChannels != AUDIO_CHANNELS)
  {
    qDebug() << "[MediaCapture] 音频格式与配置不同，重新创建 AudioSource";
    qDebug() << "[MediaCapture] 实际格式: 采样率=" << actualSampleRate
             << "声道=" << actualChannels;

    // 重新创建 AudioSource，使用实际格式
    m_lkAudioSource = std::make_shared<livekit::AudioSource>(
        actualSampleRate, actualChannels, 0); // 实时模式

    // 重新创建 AudioFrameHandler，使用实际格式
    m_audioHandler = std::make_unique<AudioFrameHandler>(actualSampleRate,
                                                         actualChannels, this);
    m_audioHandler->setAudioSource(m_lkAudioSource);
    if (m_apm)
    {
      m_audioHandler->setAPM(m_apm.get());
    }
    // 转发原始 PCM 数据信号（供 AI 语音转录）
    connect(m_audioHandler.get(), &AudioFrameHandler::rawAudioCaptured, this,
            &MediaCapture::rawAudioCaptured);

    // 重新创建音频轨道
    m_lkAudioTrack = livekit::LocalAudioTrack::createLocalAudioTrack(
        "microphone", m_lkAudioSource);
    qDebug()
        << "[MediaCapture] AudioSource 和 AudioTrack 已使用实际格式重新创建";
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

void MediaCapture::stopMicrophone()
{
  if (!m_microphoneActive)
  {
    return;
  }

  qDebug() << "[MediaCapture] 停止麦克风...";

  m_audioHandler->setEnabled(false);

  if (m_audioInput)
  {
    m_audioInput->stop();
    m_audioInput.reset();
  }

  m_microphoneActive = false;
  emit microphoneActiveChanged();

  qDebug() << "[MediaCapture] 麦克风已停止";
}

void MediaCapture::toggleMicrophone()
{
  if (m_microphoneActive)
  {
    stopMicrophone();
  }
  else
  {
    startMicrophone();
  }
}

// =============================================================================
// 内部槽函数
// =============================================================================

void MediaCapture::onCameraActiveChanged(bool active)
{
  qDebug() << "[MediaCapture] 摄像头活动状态变化:" << active;
  m_cameraActive = active;
  emit cameraActiveChanged();
}

void MediaCapture::onCameraErrorOccurred(QCamera::Error error,
                                         const QString &errorString)
{
  qWarning() << "[MediaCapture] 摄像头错误:" << error << errorString;
  emit cameraError(errorString);
}

void MediaCapture::onVideoFrameReceived(const QVideoFrame &frame)
{
  // 转发到处理器（处理器会推送到 LiveKit）
  m_videoHandler->handleVideoFrame(frame);

  // 【关键修复】将 QPointer 复制到局部变量，确保在整个使用期间指针有效
  // QPointer 的检查和使用不是原子操作，QML 对象可能在检查后、使用前被销毁
  // 复制到局部变量后，即使 m_externalVideoSink 变为 null，局部变量仍然持有引用
  QVideoSink *externalSink = m_externalVideoSink.data();

  // 如果有外部 sink，也发送帧
  if (externalSink && externalSink != m_internalVideoSink.get())
  {
    externalSink->setVideoFrame(frame);

    // 每100帧打印一次调试信息
    static int frameCount = 0;
    if (++frameCount % 100 == 0)
    {
      qDebug() << "[MediaCapture] 已发送" << frameCount << "帧到外部 VideoSink";
    }
  }

  emit videoFrameCaptured();
}
