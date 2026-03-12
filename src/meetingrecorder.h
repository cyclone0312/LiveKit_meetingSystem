/**
 * @file meetingrecorder.h
 * @brief 会议视频录制器
 *
 * 负责：
 * 1. 接收 VideoCompositor 输出的合成视频帧
 * 2. 接收 AudioMixer 输出的混合音频数据
 * 3. 使用 FFmpeg 将音视频编码为单个 MP4 文件（H.264 + AAC）
 * 4. 编码运行在后台线程中，不阻塞 UI
 */

#ifndef MEETINGRECORDER_H
#define MEETINGRECORDER_H

#include <QElapsedTimer>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QThread>
#include <QQueue>
#include <QWaitCondition>
#include <atomic>
#include <memory>

// FFmpeg headers (C API)
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

class MeetingRecorder : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isRecording READ isRecording NOTIFY recordingChanged)
    Q_PROPERTY(int durationSeconds READ durationSeconds NOTIFY durationChanged)

public:
    explicit MeetingRecorder(QObject *parent = nullptr);
    ~MeetingRecorder() override;

    bool isRecording() const { return m_recording.load(); }
    int durationSeconds() const { return m_durationSeconds.load(); }

public slots:
    /**
     * @brief 开始录制
     * @param outputPath MP4 输出路径
     * @param width 视频宽度（默认 1920）
     * @param height 视频高度（默认 1080）
     * @param fps 帧率（默认 30）
     * @param audioSampleRate 音频采样率（默认 48000）
     * @return true 成功开始录制
     */
    bool startRecording(const QString &outputPath, int width = 1920,
                        int height = 1080, int fps = 30,
                        int audioSampleRate = 48000);

    /**
     * @brief 停止录制并关闭文件
     */
    void stopRecording();

    /**
     * @brief 输入合成视频帧
     * @param frame ARGB32 格式的 QImage
     * @param timestampUs 时间戳（微秒）
     */
    void feedVideoFrame(const QImage &frame, qint64 timestampUs);

    /**
     * @brief 输入混合音频数据
     * @param pcmData int16_t PCM 数据
     * @param sampleRate 采样率
     * @param channels 声道数
     */
    void feedAudioData(const QByteArray &pcmData, int sampleRate, int channels);

signals:
    void recordingChanged();
    void durationChanged();
    void errorOccurred(const QString &error);
    void recordingStopped(const QString &filePath);

private:
    // 编码线程入口
    void encodingLoop();

    // 初始化 FFmpeg 输出上下文和编码器
    bool initFFmpeg(const QString &outputPath, int width, int height, int fps,
                    int audioSampleRate);
    void cleanupFFmpeg();

    // 编码单帧视频
    bool encodeVideoFrame(const QImage &frame, qint64 timestampUs);
    // 编码音频数据
    bool encodeAudioSamples(const int16_t *samples, int sampleCount);
    // flush 编码器
    void flushEncoders();

private:
    std::atomic<bool> m_recording{false};
    std::atomic<int> m_durationSeconds{0};
    QString m_outputPath;

    // 编码线程
    QThread *m_encodingThread = nullptr;

    // 线程安全的帧队列
    QMutex m_videoMutex;
    QQueue<QPair<QImage, qint64>> m_videoQueue;

    QMutex m_audioMutex;
    QByteArray m_audioBuffer;

    QWaitCondition m_encodeCondition;
    QMutex m_conditionMutex;

    // FFmpeg 上下文
    AVFormatContext *m_formatCtx = nullptr;

    // 视频编码
    AVCodecContext *m_videoCodecCtx = nullptr;
    AVStream *m_videoStream = nullptr;
    SwsContext *m_swsCtx = nullptr;
    AVFrame *m_videoFrame = nullptr;
    int64_t m_videoFrameCount = 0;
    int64_t m_lastVideoPts = -1; // 保证 PTS 严格单调递增
    int m_videoWidth = 1920;
    int m_videoHeight = 1080;
    int m_videoFps = 30;
    static constexpr int VIDEO_TIME_BASE = 90000; // MPEG 标准高精度时间基

    // 音频编码
    AVCodecContext *m_audioCodecCtx = nullptr;
    AVStream *m_audioStream = nullptr;
    SwrContext *m_swrCtx = nullptr;
    AVFrame *m_audioFrame = nullptr;
    int64_t m_audioSampleCount = 0;
    int m_audioSampleRate = 48000;
    int m_audioFrameSize = 0; // AAC encoder 一帧的 sample 数

    // 音频临时缓冲（积攒到 m_audioFrameSize 后送编码）
    QByteArray m_audioEncodeBuf;

    // 音视频同步：共享挂钟
    QElapsedTimer m_wallClock;
    bool m_audioTimeInitialized = false;

    // 开始时间
    qint64 m_startTimeUs = 0;
};

#endif // MEETINGRECORDER_H
