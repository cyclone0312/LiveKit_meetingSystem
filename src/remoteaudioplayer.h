/**
 * @file remoteaudioplayer.h
 * @brief 远程音频播放器
 *
 * 负责从 LiveKit 远程音频轨道读取音频帧，
 * 使用 QAudioSink 播放到扬声器。
 */

#ifndef REMOTEAUDIOPLAYER_H
#define REMOTEAUDIOPLAYER_H

#include <QAudioFormat>
#include <QAudioSink>
#include <QBuffer>
#include <QIODevice>
#include <QMutex>
#include <QObject>
#include <QTimer>
#include <atomic>
#include <memory>
#include <queue>
#include <thread>

// LiveKit SDK
#include <livekit/audio_frame.h>
#include <livekit/audio_stream.h>
#include <livekit/track.h>

/**
 * @brief 远程音频播放器
 *
 * 从 LiveKit 远程音频轨道读取帧并通过 QAudioSink 播放
 * 【重要】QAudioSink 必须在主线程创建和操作
 */
class RemoteAudioPlayer : public QObject {
  Q_OBJECT

public:
  explicit RemoteAudioPlayer(std::shared_ptr<livekit::Track> audioTrack,
                             QObject *parent = nullptr);
  ~RemoteAudioPlayer();

  /**
   * @brief 获取关联的参会者 ID
   */
  QString participantId() const { return m_participantId; }
  void setParticipantId(const QString &id) { m_participantId = id; }

  /**
   * @brief 启动播放
   */
  void start();

  /**
   * @brief 停止播放
   */
  void stop();

  /**
   * @brief 是否正在运行
   */
  bool isRunning() const { return m_running.load(); }

  /**
   * @brief 设置静音状态（对端 mute 时暂停处理音频帧）
   */
  void setMuted(bool muted);

  /**
   * @brief 设置音量 (0.0 - 1.0)
   */
  void setVolume(float volume);
  float volume() const { return m_volume; }

signals:
  void errorOccurred(const QString &error);
  // 内部信号：用于跨线程通信
  void audioDataReady(QByteArray data, int sampleRate, int channels);

private slots:
  /**
   * @brief 在主线程处理音频数据
   */
  void onAudioDataReady(QByteArray data, int sampleRate, int channels);

private:
  /**
   * @brief 播放循环（在工作线程中运行）
   */
  void playbackLoop();

  /**
   * @brief 初始化音频输出设备（必须在主线程调用）
   */
  bool initAudioOutput(int sampleRate, int channels);

private:
  // LiveKit 组件
  std::shared_ptr<livekit::Track> m_track;
  std::shared_ptr<livekit::AudioStream> m_audioStream;

  // Qt 音频输出（必须在主线程操作）
  std::unique_ptr<QAudioSink> m_audioSink;
  QIODevice *m_audioDevice{nullptr};

  // 状态
  std::atomic<bool> m_running{false};
  std::atomic<bool> m_muted{false}; // 对端是否静音
  QString m_participantId;
  float m_volume{1.0f};
  bool m_audioInitialized{false};

  // 音频参数
  int m_sampleRate{48000};
  int m_channels{1};

  // 播放线程
  std::thread m_playbackThread;

  // 互斥锁
  QMutex m_mutex;
};

#endif // REMOTEAUDIOPLAYER_H
