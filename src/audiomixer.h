/**
 * @file audiomixer.h
 * @brief 音频混音器
 *
 * 负责：
 * 1. 接收本地麦克风 PCM 音频数据（master clock）
 * 2. 接收所有远程参会者的 PCM 音频数据
 * 3. 将多路音频逐样本相加并钳位混合
 * 4. 输出混合后的 PCM 数据供 AIAssistant 和 MeetingRecorder 使用
 *
 * 所有输入/输出统一格式：48kHz / mono / int16_t
 */

#ifndef AUDIOMIXER_H
#define AUDIOMIXER_H

#include <QByteArray>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QMap>
#include <vector>

class AudioMixer : public QObject
{
    Q_OBJECT
public:
    explicit AudioMixer(QObject *parent = nullptr);
    ~AudioMixer() override;

public slots:
    /**
     * @brief 输入本地麦克风音频（作为 master clock 驱动混音输出）
     *
     * 每次调用时，将本地数据与已缓存的远程数据混合后发出 mixedAudioReady 信号。
     */
    void feedLocalAudio(const QByteArray &pcmData, int sampleRate, int channels);

    /**
     * @brief 输入远程参会者音频
     *
     * 数据缓存到对应 participantId 的缓冲区，等待下一次 feedLocalAudio 时一起混合。
     */
    void feedRemoteAudio(const QString &participantId,
                         const QByteArray &pcmData, int sampleRate, int channels);

    /**
     * @brief 移除指定参会者的缓冲区（参会者离开时调用）
     */
    void removeParticipant(const QString &participantId);

signals:
    /**
     * @brief 混合后的 PCM 数据就绪
     * @param pcmData 混合后的 PCM 数据（48kHz / mono / int16_t）
     * @param sampleRate 采样率
     * @param channels 声道数
     */
    void mixedAudioReady(const QByteArray &pcmData, int sampleRate, int channels);

private:
    mutable QMutex m_mutex;

    // 每个远程参会者的待混合音频缓冲区
    QMap<QString, QByteArray> m_remoteBuffers;
};

#endif // AUDIOMIXER_H
