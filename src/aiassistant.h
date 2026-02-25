/**
 * @file aiassistant.h
 * @brief AI 会议助手 + 语音转录管理器
 *
 * 负责：
 * 1. 调用后端 AI 接口进行对话、总结、翻译等
 * 2. 录制麦克风 PCM 数据，定时编码为 WAV 发往后端做语音转录
 * 3. 管理转录历史，供会议纪要生成使用
 */

#ifndef AIASSISTANT_H
#define AIASSISTANT_H

#include <QBuffer>
#include <QByteArray>
#include <QDateTime>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

/**
 * @brief 一条转录记录
 */
struct TranscriptEntry
{
    QString time;    // 时间戳 "HH:mm:ss"
    QString speaker; // 发言者（如果可识别）
    QString text;    // 转录文本
};

class AIAssistant : public QObject
{
    Q_OBJECT

    // ====== AI 对话相关属性 ======
    Q_PROPERTY(bool isBusy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(QString lastReply READ lastReply NOTIFY lastReplyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

    // ====== 语音转录相关属性 ======
    Q_PROPERTY(bool isTranscribing READ isTranscribing NOTIFY transcribingChanged)
    Q_PROPERTY(
        QString liveTranscript READ liveTranscript NOTIFY liveTranscriptChanged)
    Q_PROPERTY(int transcriptCount READ transcriptCount NOTIFY
                   transcriptCountChanged)

    // ====== 配置 ======
    Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY
                   serverUrlChanged)
    Q_PROPERTY(QString roomName READ roomName WRITE setRoomName NOTIFY
                   roomNameChanged)
    Q_PROPERTY(
        QString userName READ userName WRITE setUserName NOTIFY userNameChanged)

public:
    explicit AIAssistant(QObject *parent = nullptr);
    ~AIAssistant();

    // ====== 属性 Getter ======
    bool isBusy() const;
    QString lastReply() const;
    QString lastError() const;
    bool isTranscribing() const;
    QString liveTranscript() const;
    int transcriptCount() const;
    QString serverUrl() const;
    QString roomName() const;
    QString userName() const;

    // ====== 属性 Setter ======
    void setServerUrl(const QString &url);
    void setRoomName(const QString &name);
    void setUserName(const QString &name);

public slots:
    // ====== AI 对话接口（QML 可调用） ======

    /**
     * @brief 向 AI 发送消息
     * @param message 用户消息内容
     */
    void sendMessage(const QString &message);

    /**
     * @brief 根据聊天记录生成摘要
     * @param chatHistory QVariantList，每项含 sender/content/time
     */
    void summarize(const QVariantList &chatHistory);

    /**
     * @brief 综合生成会议纪要（语音转录 + 聊天记录）
     * @param chatHistory 聊天记录
     */
    void generateMeetingMinutes(const QVariantList &chatHistory);

    /**
     * @brief 清除当前房间的 AI 对话历史
     */
    void clearAIHistory();

    // ====== 语音转录控制 ======

    /**
     * @brief 开始语音转录（启动定时录制+发送）
     */
    void startTranscription();

    /**
     * @brief 停止语音转录
     */
    void stopTranscription();

    /**
     * @brief 接收 PCM 音频数据（由 AudioFrameHandler 或外部调用）
     * @param pcmData 原始 PCM 数据（16-bit, mono, 48kHz）
     * @param sampleRate 采样率
     * @param channels 声道数
     */
    void feedAudioData(const QByteArray &pcmData, int sampleRate, int channels);

    /**
     * @brief 获取所有转录记录（供会议纪要使用）
     */
    QVariantList getTranscripts() const;

    /**
     * @brief 清除转录历史
     */
    void clearTranscripts();

signals:
    // AI 对话信号
    void busyChanged();
    void lastReplyChanged();
    void lastErrorChanged();
    void aiReplyReceived(const QString &reply);
    void aiError(const QString &error);

    // 会议纪要/摘要信号
    void summaryReceived(const QString &summary);
    void meetingMinutesReceived(const QString &minutes);

    // 语音转录信号
    void transcribingChanged();
    void liveTranscriptChanged();
    void transcriptCountChanged();
    void newTranscriptReceived(const QString &text);

    // 配置信号
    void serverUrlChanged();
    void roomNameChanged();
    void userNameChanged();

private slots:
    /**
     * @brief 定时器触发：将缓冲的音频数据打包发送
     */
    void onTranscriptionTimerTick();

private:
    /**
     * @brief 将 PCM 数据编码为 WAV 格式
     */
    QByteArray encodePcmToWav(const QByteArray &pcmData, int sampleRate,
                              int channels, int bitsPerSample = 16);

    /**
     * @brief 发起 POST 请求到后端
     */
    void postRequest(const QString &endpoint, const QJsonObject &body,
                     std::function<void(bool, const QJsonObject &)> callback);

    /**
     * @brief 设置错误信息
     */
    void setError(const QString &error);

    /**
     * @brief 设置忙碌状态
     */
    void setBusy(bool busy);

private:
    QNetworkAccessManager *m_networkManager;

    // 配置
    QString m_serverUrl;
    QString m_roomName;
    QString m_userName;

    // AI 对话状态
    bool m_isBusy;
    QString m_lastReply;
    QString m_lastError;

    // 语音转录
    bool m_isTranscribing;
    QTimer *m_transcriptionTimer;
    QByteArray m_audioBuffer;             // PCM 数据缓冲区
    QMutex m_audioBufferMutex;            // 缓冲区线程安全锁
    int m_audioSampleRate;                // 当前缓冲中数据的采样率
    int m_audioChannels;                  // 当前缓冲中数据的声道数
    QString m_liveTranscript;             // 最新的一段转录文本
    QList<TranscriptEntry> m_transcripts; // 全部转录历史

    // 转录间隔（毫秒）
    static const int TRANSCRIPTION_INTERVAL_MS = 30000; // 30 秒
    // 最小音频数据量（字节），低于此值不发送（避免静音片段浪费 API 调用）
    static const int MIN_AUDIO_BYTES = 48000 * 2 * 2; // 约 2 秒的 48kHz 16bit mono
};

#endif // AIASSISTANT_H
