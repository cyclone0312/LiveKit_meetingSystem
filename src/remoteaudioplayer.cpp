/**
 * @file remoteaudioplayer.cpp
 * @brief 远程音频播放器实现
 *
 * 【重要】QAudioSink 必须在主线程创建和操作
 * 本实现使用信号槽机制将音频数据从工作线程传递到主线程播放
 */

#include "remoteaudioplayer.h"
#include <QAudioDevice>
#include <QDebug>
#include <QMediaDevices>
#include <thread>


RemoteAudioPlayer::RemoteAudioPlayer(std::shared_ptr<livekit::Track> audioTrack,
                                     QObject *parent)
    : QObject(parent), m_track(audioTrack) {
  // 连接信号槽，确保音频数据在主线程处理
  connect(this, &RemoteAudioPlayer::audioDataReady, this,
          &RemoteAudioPlayer::onAudioDataReady, Qt::QueuedConnection);

  qDebug() << "[RemoteAudioPlayer] 创建远程音频播放器";
}

RemoteAudioPlayer::~RemoteAudioPlayer() {
  stop();
  qDebug() << "[RemoteAudioPlayer] 销毁远程音频播放器";
}

void RemoteAudioPlayer::start() {
  if (m_running.load()) {
    qDebug() << "[RemoteAudioPlayer] 已在运行中";
    return;
  }

  if (!m_track) {
    emit errorOccurred("音频轨道无效");
    return;
  }

  qDebug() << "[RemoteAudioPlayer] 启动播放";

  // 创建音频流
  livekit::AudioStream::Options options;
  options.capacity = 10; // 缓冲 10 帧

  try {
    m_audioStream = livekit::AudioStream::fromTrack(m_track, options);
    if (!m_audioStream) {
      emit errorOccurred("无法创建音频流");
      return;
    }
  } catch (const std::exception &e) {
    emit errorOccurred(QString("创建音频流失败: %1").arg(e.what()));
    return;
  }

  m_running.store(true);
  m_audioInitialized = false;

  // 在后台线程中运行播放循环（保存线程引用以便 stop 时 join）
  m_playbackThread = std::thread([this]() { playbackLoop(); });
}

void RemoteAudioPlayer::stop() {
  if (!m_running.load()) {
    return;
  }

  qDebug() << "[RemoteAudioPlayer] 停止播放";
  m_running.store(false);

  // 关闭音频流（这会唤醒阻塞的 read() 调用）
  if (m_audioStream) {
    m_audioStream->close();
    m_audioStream.reset();
  }

  // 等待播放线程结束
  if (m_playbackThread.joinable()) {
    m_playbackThread.join();
  }

  // 停止音频输出（在主线程中安全操作）
  QMutexLocker locker(&m_mutex);
  if (m_audioSink) {
    m_audioSink->stop();
    m_audioSink.reset();
    m_audioDevice = nullptr;
  }
  m_audioInitialized = false;
}

void RemoteAudioPlayer::setVolume(float volume) {
  m_volume = qBound(0.0f, volume, 1.0f);
  QMutexLocker locker(&m_mutex);
  if (m_audioSink) {
    m_audioSink->setVolume(m_volume);
  }
}

bool RemoteAudioPlayer::initAudioOutput(int sampleRate, int channels) {
  // 注意：此方法应该在主线程调用
  QMutexLocker locker(&m_mutex);

  // 如果已经初始化且参数相同，跳过
  if (m_audioSink && m_sampleRate == sampleRate && m_channels == channels) {
    return true;
  }

  // 清理旧的
  if (m_audioSink) {
    m_audioSink->stop();
    m_audioSink.reset();
    m_audioDevice = nullptr;
  }

  // 设置音频格式
  QAudioFormat format;
  format.setSampleRate(sampleRate);
  format.setChannelCount(channels);
  format.setSampleFormat(QAudioFormat::Int16); // LiveKit 使用 int16 PCM

  // 获取默认输出设备
  QAudioDevice outputDevice = QMediaDevices::defaultAudioOutput();
  if (outputDevice.isNull()) {
    qWarning() << "[RemoteAudioPlayer] 没有可用的音频输出设备";
    return false;
  }

  // 检查格式是否支持
  if (!outputDevice.isFormatSupported(format)) {
    qWarning() << "[RemoteAudioPlayer] 音频格式不支持:"
               << "采样率=" << sampleRate << "声道=" << channels;
    // 可以尝试修改格式，但先继续尝试
  }

  // 创建音频输出
  m_audioSink = std::make_unique<QAudioSink>(outputDevice, format);
  m_audioSink->setVolume(m_volume);

  // 设置较大的缓冲区以避免断续
  m_audioSink->setBufferSize(sampleRate * channels * sizeof(int16_t) /
                             5); // 200ms 缓冲

  // 启动音频输出
  m_audioDevice = m_audioSink->start();
  if (!m_audioDevice) {
    qWarning() << "[RemoteAudioPlayer] 无法启动音频输出";
    m_audioSink.reset();
    return false;
  }

  m_sampleRate = sampleRate;
  m_channels = channels;

  qDebug() << "[RemoteAudioPlayer] 音频输出初始化成功:"
           << "采样率=" << sampleRate << "声道=" << channels
           << "缓冲区=" << m_audioSink->bufferSize();

  return true;
}

void RemoteAudioPlayer::onAudioDataReady(QByteArray data, int sampleRate,
                                         int channels) {
  // 此槽在主线程执行
  if (!m_running.load()) {
    return;
  }

  // 首次收到数据时初始化音频输出
  if (!m_audioInitialized) {
    qDebug() << "[RemoteAudioPlayer] 首次收到音频数据，初始化输出..."
             << "采样率=" << sampleRate << "声道=" << channels
             << "数据大小=" << data.size();
    if (!initAudioOutput(sampleRate, channels)) {
      qWarning() << "[RemoteAudioPlayer] 音频输出初始化失败";
      return;
    }
    m_audioInitialized = true;
  }

  // 检查音频设备状态
  if (!m_audioDevice || !m_audioSink) {
    return;
  }

  // 写入音频数据
  QMutexLocker locker(&m_mutex);
  if (m_audioDevice && m_audioSink) {
    qint64 written = m_audioDevice->write(data);

    // 每 100 次写入打印一次状态
    static int writeCount = 0;
    if (++writeCount % 100 == 1) {
      qDebug() << "[RemoteAudioPlayer] 写入音频数据:"
               << "写入=" << written << "/" << data.size()
               << "状态=" << m_audioSink->state()
               << "音量=" << m_audioSink->volume();
    }
  }
}

void RemoteAudioPlayer::playbackLoop() {
  qDebug() << "[RemoteAudioPlayer] 播放循环开始";

  int frameCount = 0;

  while (m_running.load() && m_audioStream) {
    livekit::AudioFrameEvent event;

    // 阻塞读取音频帧
    bool hasFrame = m_audioStream->read(event);

    if (!hasFrame) {
      // 流已结束或被关闭
      qDebug() << "[RemoteAudioPlayer] 音频流结束";
      break;
    }

    if (!m_running.load()) {
      break;
    }

    frameCount++;
    const livekit::AudioFrame &frame = event.frame;

    // 每 100 帧打印一次日志
    if (frameCount % 100 == 1) {
      qDebug() << "[RemoteAudioPlayer] 收到音频帧 #" << frameCount
               << "采样率=" << frame.sample_rate()
               << "声道=" << frame.num_channels()
               << "样本数=" << frame.data().size();
    }

    // 将音频数据发送到主线程处理
    const std::vector<int16_t> &samples = frame.data();
    if (!samples.empty()) {
      // 转换为 QByteArray
      QByteArray audioData(reinterpret_cast<const char *>(samples.data()),
                           static_cast<int>(samples.size() * sizeof(int16_t)));

      // 发送信号到主线程（Qt::QueuedConnection）
      emit audioDataReady(audioData, frame.sample_rate(), frame.num_channels());
    }
  }

  qDebug() << "[RemoteAudioPlayer] 播放循环结束, 共处理" << frameCount << "帧";
}
