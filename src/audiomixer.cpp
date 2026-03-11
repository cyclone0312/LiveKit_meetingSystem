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
    qDebug() << "[AudioMixer] 初始化完成";
}

AudioMixer::~AudioMixer()
{
    qDebug() << "[AudioMixer] 销毁";
}

void AudioMixer::feedLocalAudio(const QByteArray &pcmData, int sampleRate,
                                int channels)
{
    // 本地麦克风数据到达 → 作为 master clock 触发一次混音

    const int localSampleCount =
        static_cast<int>(pcmData.size()) / static_cast<int>(sizeof(int16_t));
    if (localSampleCount <= 0)
        return;

    const auto *localSamples =
        reinterpret_cast<const int16_t *>(pcmData.constData());

    // 用 int32 临时缓冲来累加，防止溢出
    std::vector<int32_t> mixBuf(localSampleCount);
    for (int i = 0; i < localSampleCount; ++i)
    {
        mixBuf[i] = static_cast<int32_t>(localSamples[i]);
    }

    // 叠加所有远程参会者的缓冲区
    {
        QMutexLocker locker(&m_mutex);
        for (auto it = m_remoteBuffers.begin(); it != m_remoteBuffers.end();
             ++it)
        {
            QByteArray &remoteBuf = it.value();
            if (remoteBuf.isEmpty())
                continue;

            const auto *remoteSamples =
                reinterpret_cast<const int16_t *>(remoteBuf.constData());
            const int remoteSampleCount =
                static_cast<int>(remoteBuf.size()) /
                static_cast<int>(sizeof(int16_t));

            // 取两者中较短的长度进行混合
            const int mixLen = qMin(localSampleCount, remoteSampleCount);
            for (int i = 0; i < mixLen; ++i)
            {
                mixBuf[i] += static_cast<int32_t>(remoteSamples[i]);
            }

            // 消费掉已混合的部分，保留多出来的数据等待下一轮
            const int consumedBytes =
                qMin(localSampleCount, remoteSampleCount) *
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
        outSamples[i] = static_cast<int16_t>(
            qBound(static_cast<int32_t>(-32768), mixBuf[i],
                   static_cast<int32_t>(32767)));
    }

    emit mixedAudioReady(output, sampleRate, channels);
}

void AudioMixer::feedRemoteAudio(const QString &participantId,
                                 const QByteArray &pcmData, int sampleRate,
                                 int channels)
{
    Q_UNUSED(sampleRate)
    Q_UNUSED(channels)

    if (pcmData.isEmpty())
        return;

    QMutexLocker locker(&m_mutex);
    m_remoteBuffers[participantId].append(pcmData);

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
