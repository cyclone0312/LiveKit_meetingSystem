/**
 * @file audiomixer.cpp
 * @brief 音频混音器实现
 */

#include "audiomixer.h"
#include <QDebug>
#include <QtGlobal>
#include <cstring>

AudioMixer::AudioMixer(QObject *parent) : QObject(parent)
{
  // 定时混音：当本地麦克风未提供数据时，仍然输出远程音频
  m_mixTimer = new QTimer(this);
  m_mixTimer->setInterval(MIX_INTERVAL_MS);
  connect(m_mixTimer, &QTimer::timeout, this, &AudioMixer::onMixTimer);
  m_mixTimer->start();

  qDebug() << "[AudioMixer] 初始化完成";
}

AudioMixer::~AudioMixer() { qDebug() << "[AudioMixer] 销毁"; }

void AudioMixer::feedLocalAudio(const QByteArray &pcmData, int sampleRate,
                                int channels)
{
  // 本地麦克风数据到达 → 作为 master clock 触发一次混音
  m_localAudioTimer.restart();
  m_outputSampleRate = sampleRate;

  // 【关键修复】统一下混为单声道，防止立体声数据被误当做单声道处理导致乱码
  const QByteArray &monoData =
      (channels > 1) ? downmixToMono(pcmData, channels) : pcmData;

  const int localSampleCount =
      static_cast<int>(monoData.size()) / static_cast<int>(sizeof(int16_t));
  if (localSampleCount <= 0)
    return;

  const auto *localSamples =
      reinterpret_cast<const int16_t *>(monoData.constData());

  // 用 int32 临时缓冲来累加，防止溢出
  std::vector<int32_t> mixBuf(localSampleCount);
  for (int i = 0; i < localSampleCount; ++i)
  {
    mixBuf[i] = static_cast<int32_t>(localSamples[i]);
  }

  // 叠加所有远程参会者的缓冲区
  {
    QMutexLocker locker(&m_mutex);
    for (auto it = m_remoteBuffers.begin(); it != m_remoteBuffers.end(); ++it)
    {
      QByteArray &remoteBuf = it.value();
      if (remoteBuf.isEmpty())
        continue;

      const auto *remoteSamples =
          reinterpret_cast<const int16_t *>(remoteBuf.constData());
      const int remoteSampleCount = static_cast<int>(remoteBuf.size()) /
                                    static_cast<int>(sizeof(int16_t));

      // 取两者中较短的长度进行混合
      const int mixLen = qMin(localSampleCount, remoteSampleCount);
      for (int i = 0; i < mixLen; ++i)
      {
        mixBuf[i] += static_cast<int32_t>(remoteSamples[i]);
      }

      // 消费掉已混合的部分，保留多出来的数据等待下一轮
      const int consumedBytes = qMin(localSampleCount, remoteSampleCount) *
                                static_cast<int>(sizeof(int16_t));
      remoteBuf.remove(0, consumedBytes);
    }
  }

  // 钳位到 int16 范围
  QByteArray output(localSampleCount * static_cast<int>(sizeof(int16_t)),
                    Qt::Uninitialized);
  auto *outSamples = reinterpret_cast<int16_t *>(output.data());
  for (int i = 0; i < localSampleCount; ++i)
  {
    outSamples[i] = static_cast<int16_t>(qBound(
        static_cast<int32_t>(-32768), mixBuf[i], static_cast<int32_t>(32767)));
  }

  // 始终输出单声道，与下游 MeetingRecorder/AIAssistant 的预期格式一致
  emit mixedAudioReady(output, sampleRate, 1);
}

void AudioMixer::feedRemoteAudio(const QString &participantId,
                                 const QByteArray &pcmData, int sampleRate,
                                 int channels)
{
  Q_UNUSED(sampleRate)

  if (pcmData.isEmpty())
    return;

  // 【关键修复】统一下混为单声道
  const QByteArray &monoData =
      (channels > 1) ? downmixToMono(pcmData, channels) : pcmData;

  QMutexLocker locker(&m_mutex);
  m_remoteBuffers[participantId].append(monoData);

  // 安全阀：防止某个远程流堆积过多数据（超过 2 秒 = 48000*2*2 bytes）
  static constexpr int MAX_BUFFER_BYTES = 48000 * 2 * 2;
  QByteArray &buf = m_remoteBuffers[participantId];
  if (buf.size() > MAX_BUFFER_BYTES)
  {
    buf.remove(0, buf.size() - MAX_BUFFER_BYTES);
  }
}

void AudioMixer::removeParticipant(const QString &participantId)
{
  QMutexLocker locker(&m_mutex);
  m_remoteBuffers.remove(participantId);
  qDebug() << "[AudioMixer] 移除参会者缓冲:" << participantId;
}

QByteArray AudioMixer::downmixToMono(const QByteArray &pcmData, int channels)
{
  if (channels <= 1)
    return pcmData;

  const auto *src =
      reinterpret_cast<const int16_t *>(pcmData.constData());
  const int totalSamples =
      static_cast<int>(pcmData.size()) / static_cast<int>(sizeof(int16_t));
  const int monoSamples = totalSamples / channels;

  QByteArray mono(monoSamples * static_cast<int>(sizeof(int16_t)),
                  Qt::Uninitialized);
  auto *dst = reinterpret_cast<int16_t *>(mono.data());

  for (int i = 0; i < monoSamples; ++i)
  {
    int32_t sum = 0;
    for (int ch = 0; ch < channels; ++ch)
    {
      sum += src[i * channels + ch];
    }
    dst[i] = static_cast<int16_t>(
        qBound(static_cast<int32_t>(-32768), sum / channels,
               static_cast<int32_t>(32767)));
  }
  return mono;
}

void AudioMixer::onMixTimer()
{
  // 如果本地麦克风最近提供了数据，由 feedLocalAudio 驱动混音即可
  if (m_localAudioTimer.isValid() &&
      m_localAudioTimer.elapsed() < MIX_INTERVAL_MS * 2)
  {
    return;
  }

  // 本地麦克风未提供数据，检查是否有远程音频需要输出
  QMutexLocker locker(&m_mutex);

  bool hasRemoteData = false;
  for (auto it = m_remoteBuffers.cbegin(); it != m_remoteBuffers.cend(); ++it)
  {
    if (!it.value().isEmpty())
    {
      hasRemoteData = true;
      break;
    }
  }
  if (!hasRemoteData)
    return;

  // 生成 MIX_INTERVAL_MS 毫秒的静音作为本地音频，驱动远程音频输出
  const int silenceSamples = m_outputSampleRate * MIX_INTERVAL_MS / 1000;
  std::vector<int32_t> mixBuf(silenceSamples, 0);

  for (auto it = m_remoteBuffers.begin(); it != m_remoteBuffers.end(); ++it)
  {
    QByteArray &remoteBuf = it.value();
    if (remoteBuf.isEmpty())
      continue;

    const auto *remoteSamples =
        reinterpret_cast<const int16_t *>(remoteBuf.constData());
    const int remoteSampleCount = static_cast<int>(remoteBuf.size()) /
                                  static_cast<int>(sizeof(int16_t));

    const int mixLen = qMin(silenceSamples, remoteSampleCount);
    for (int i = 0; i < mixLen; ++i)
    {
      mixBuf[i] += static_cast<int32_t>(remoteSamples[i]);
    }

    const int consumedBytes = mixLen * static_cast<int>(sizeof(int16_t));
    remoteBuf.remove(0, consumedBytes);
  }

  QByteArray output(silenceSamples * static_cast<int>(sizeof(int16_t)),
                    Qt::Uninitialized);
  auto *outSamples = reinterpret_cast<int16_t *>(output.data());
  for (int i = 0; i < silenceSamples; ++i)
  {
    outSamples[i] = static_cast<int16_t>(
        qBound(static_cast<int32_t>(-32768), mixBuf[i],
               static_cast<int32_t>(32767)));
  }

  emit mixedAudioReady(output, m_outputSampleRate, 1);
}
