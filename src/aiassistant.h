/**
 * @file aiassistant.h
 * @brief AI 会议助手 + 录音转文字管理器
 *
 * 负责：
 * 1. 调用后端 AI 接口进行对话、总结、翻译等
 * 2. 本地录制麦克风音频（PCM 缓冲）
 * 3. 停止录制后，将音频组装为 WAV 发送到后端进行离线 ASR 转录
 * 4. 管理转录历史，供会议纪要生成使用
 * 5. 本地录音文件管理（保存/读取/删除/转录/生成纪要）
 */

#ifndef AIASSISTANT_H
#define AIASSISTANT_H

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QObject>
#include <QSettings>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

/**
 * @brief 一条转录记录
 */
struct TranscriptEntry {
  QString time;    // 时间戳 "HH:mm:ss"
  QString speaker; // 发言者
  QString text;    // 转录文本
};

class AIAssistant : public QObject {
  Q_OBJECT

  // ====== AI 对话相关属性 ======
  Q_PROPERTY(bool isBusy READ isBusy NOTIFY busyChanged)
  Q_PROPERTY(QString lastReply READ lastReply NOTIFY lastReplyChanged)
  Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

  // ====== 语音转录相关属性 ======
  Q_PROPERTY(
      bool isRecordingAudio READ isRecordingAudio NOTIFY recordingAudioChanged)
  Q_PROPERTY(bool isTranscribing READ isTranscribing NOTIFY transcribingChanged)
  Q_PROPERTY(
      int transcriptCount READ transcriptCount NOTIFY transcriptCountChanged)
  Q_PROPERTY(QString recordingDuration READ recordingDuration NOTIFY
                 recordingDurationChanged)

  // ====== 本地录音文件管理 ======
  Q_PROPERTY(QVariantList localRecordings READ localRecordings NOTIFY
                 localRecordingsChanged)

  // ====== 配置 ======
  Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY
                 serverUrlChanged)
  Q_PROPERTY(
      QString roomName READ roomName WRITE setRoomName NOTIFY roomNameChanged)
  Q_PROPERTY(
      QString userName READ userName WRITE setUserName NOTIFY userNameChanged)

public:
  explicit AIAssistant(QObject *parent = nullptr);
  ~AIAssistant();

  // ====== 属性 Getter ======
  bool isBusy() const;
  QString lastReply() const;
  QString lastError() const;
  bool isRecordingAudio() const;
  bool isTranscribing() const;
  int transcriptCount() const;
  QString recordingDuration() const;
  QVariantList localRecordings() const;
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

  // ====== 录音 + 离线转录控制 ======

  /**
   * @brief 开始录制音频（本地缓冲 PCM 数据）
   */
  void startRecording();

  /**
   * @brief 停止录制并提交转录（将录音组装为 WAV 发到服务端进行离线 ASR）
   */
  void stopRecordingAndTranscribe();

  /**
   * @brief 接收 PCM 音频数据（由 MediaCapture 回调）
   * @param pcmData 原始 PCM 数据（16-bit, mono, 48kHz）
   * @param sampleRate 采样率
   * @param channels 声道数
   *
   * 在录音模式下，数据降采样到 16kHz 后追加到本地缓冲区
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

  // ====== 本地录音文件管理 ======

  /**
   * @brief 保存当前录音缓冲到本地文件（不转录）
   * 在停止录制按钮联动时调用
   */
  Q_INVOKABLE void saveRecordingToFile();

  /**
   * @brief 转录指定的本地录音文件
   * @param filePath WAV 文件绝对路径
   */
  Q_INVOKABLE void transcribeLocalFile(const QString &filePath);

  /**
   * @brief 从本地录音文件生成会议纪要
   * @param filePath WAV 文件绝对路径
   */
  Q_INVOKABLE void generateMinutesFromFile(const QString &filePath);

  /**
   * @brief 删除指定索引的本地录音
   * @param index 录音在列表中的索引
   */
  Q_INVOKABLE void deleteLocalRecording(int index);

  /**
   * @brief 加载本地录音列表
   */
  Q_INVOKABLE void loadLocalRecordings();

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
  void recordingAudioChanged();
  void transcribingChanged();
  void transcriptCountChanged();
  void recordingDurationChanged();
  void newTranscriptReceived(const QString &text);
  void transcriptionCompleted(const QString &fullText);
  void transcriptionFailed(const QString &error);

  // 本地录音管理信号
  void localRecordingsChanged();
  void recordingSaved(const QString &filePath);
  void fileTranscriptionCompleted(const QString &filePath,
                                  const QString &transcript);
  void minutesFromFileReceived(const QString &minutes);

  // 配置信号
  void serverUrlChanged();
  void roomNameChanged();
  void userNameChanged();

private:
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

  /**
   * @brief 将 16kHz 单声道 16-bit PCM 数据组装为标准 WAV
   * @param pcmData 原始 PCM 数据
   * @param sampleRate 采样率 (16000)
   * @param channels 声道数 (1)
   * @param bitsPerSample 位深 (16)
   * @return 完整的 WAV 文件数据
   */
  QByteArray buildWavFile(const QByteArray &pcmData, int sampleRate = 16000,
                          int channels = 1, int bitsPerSample = 16) const;

  /**
   * @brief 获取本地录音存储目录
   */
  QString recordingsDir() const;

  /**
   * @brief 保存/读取录音元数据到 QSettings
   */
  void saveRecordingMeta(const QString &filePath, const QString &meetingTitle,
                         const QString &roomName, const QString &userName,
                         int durationSec, qint64 fileSize);
  void removeRecordingMeta(int index);

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

  // 本地录音状态
  bool m_isRecordingAudio;        // 是否正在录音
  bool m_isTranscribing;          // 是否正在转录中（等待服务端返回）
  QByteArray m_audioRecordBuffer; // 本地 PCM 录音缓冲（16kHz mono 16-bit）
  QTimer *m_recordingTimer;       // 录音计时器（更新时长显示）
  int m_recordingSeconds;         // 已录音秒数
  QList<TranscriptEntry> m_transcripts; // 全部已确认的转录历史

  // 本地录音文件列表缓存
  QVariantList m_localRecordings;
};

#endif // AIASSISTANT_H
