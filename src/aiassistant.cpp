#include "aiassistant.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>

AIAssistant::AIAssistant(QObject *parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this)),
      m_serverUrl("http://8.162.3.195:3000"), m_isBusy(false),
      m_isRecordingAudio(false), m_isTranscribing(false),
      m_recordingTimer(new QTimer(this)), m_recordingSeconds(0) {
  // 录音计时器：每秒更新录音时长显示
  connect(m_recordingTimer, &QTimer::timeout, this, [this]() {
    m_recordingSeconds++;
    emit recordingDurationChanged();
  });

  qDebug() << "[AIAssistant] 初始化完成 (本地录音 + 离线 ASR 模式)";
}

AIAssistant::~AIAssistant() {
  if (m_isRecordingAudio) {
    // 不提交转录，仅停止录音
    m_isRecordingAudio = false;
    m_recordingTimer->stop();
    m_audioRecordBuffer.clear();
  }
}

// ==================== 属性 Getter ====================

bool AIAssistant::isBusy() const { return m_isBusy; }
QString AIAssistant::lastReply() const { return m_lastReply; }
QString AIAssistant::lastError() const { return m_lastError; }
bool AIAssistant::isRecordingAudio() const { return m_isRecordingAudio; }
bool AIAssistant::isTranscribing() const { return m_isTranscribing; }
int AIAssistant::transcriptCount() const { return m_transcripts.count(); }
QString AIAssistant::serverUrl() const { return m_serverUrl; }
QString AIAssistant::roomName() const { return m_roomName; }
QString AIAssistant::userName() const { return m_userName; }

QString AIAssistant::recordingDuration() const {
  int minutes = m_recordingSeconds / 60;
  int seconds = m_recordingSeconds % 60;
  return QString("%1:%2")
      .arg(minutes, 2, 10, QChar('0'))
      .arg(seconds, 2, 10, QChar('0'));
}

// ==================== 属性 Setter ====================

void AIAssistant::setServerUrl(const QString &url) {
  if (m_serverUrl != url) {
    m_serverUrl = url;
    emit serverUrlChanged();
  }
}

void AIAssistant::setRoomName(const QString &name) {
  if (m_roomName != name) {
    m_roomName = name;
    emit roomNameChanged();
  }
}

void AIAssistant::setUserName(const QString &name) {
  if (m_userName != name) {
    m_userName = name;
    emit userNameChanged();
  }
}

// ==================== AI 对话接口 ====================

void AIAssistant::sendMessage(const QString &message) {
  if (message.trimmed().isEmpty() || m_roomName.isEmpty()) {
    setError("消息或房间名不能为空");
    return;
  }

  setBusy(true);

  QJsonObject body;
  body["roomName"] = m_roomName;
  body["username"] = m_userName;
  body["message"] = message;

  postRequest("/api/ai/chat", body,
              [this](bool success, const QJsonObject &json) {
                setBusy(false);
                if (success) {
                  m_lastReply = json["reply"].toString();
                  emit lastReplyChanged();
                  emit aiReplyReceived(m_lastReply);
                  qDebug() << "[AIAssistant] AI 回复:" << m_lastReply.left(80);
                } else {
                  setError(json["error"].toString("AI 请求失败"));
                }
              });
}

void AIAssistant::summarize(const QVariantList &chatHistory) {
  if (m_roomName.isEmpty()) {
    setError("房间名不能为空");
    return;
  }

  setBusy(true);

  QJsonArray historyArray;
  for (const auto &item : chatHistory) {
    QVariantMap map = item.toMap();
    QJsonObject entry;
    entry["sender"] = map["sender"].toString();
    entry["content"] = map["content"].toString();
    entry["time"] = map["time"].toString();
    historyArray.append(entry);
  }

  QJsonObject body;
  body["roomName"] = m_roomName;
  body["chatHistory"] = historyArray;

  postRequest("/api/ai/summarize", body,
              [this](bool success, const QJsonObject &json) {
                setBusy(false);
                if (success) {
                  QString summary = json["summary"].toString();
                  emit summaryReceived(summary);
                } else {
                  setError(json["error"].toString("摘要生成失败"));
                }
              });
}

void AIAssistant::generateMeetingMinutes(const QVariantList &chatHistory) {
  if (m_roomName.isEmpty()) {
    setError("房间名不能为空");
    return;
  }

  setBusy(true);

  // 构建转录数组
  QJsonArray transcriptsArray;
  for (const auto &entry : m_transcripts) {
    QJsonObject obj;
    obj["time"] = entry.time;
    obj["speaker"] = entry.speaker;
    obj["text"] = entry.text;
    transcriptsArray.append(obj);
  }

  // 构建聊天记录数组
  QJsonArray chatArray;
  for (const auto &item : chatHistory) {
    QVariantMap map = item.toMap();
    QJsonObject entry;
    entry["sender"] = map["sender"].toString();
    entry["content"] = map["content"].toString();
    entry["time"] = map["time"].toString();
    chatArray.append(entry);
  }

  QJsonObject body;
  body["roomName"] = m_roomName;
  body["transcripts"] = transcriptsArray;
  body["chatHistory"] = chatArray;

  postRequest("/api/ai/meeting-minutes", body,
              [this](bool success, const QJsonObject &json) {
                setBusy(false);
                if (success) {
                  QString minutes = json["minutes"].toString();
                  emit meetingMinutesReceived(minutes);
                } else {
                  setError(json["error"].toString("会议纪要生成失败"));
                }
              });
}

void AIAssistant::clearAIHistory() {
  if (m_roomName.isEmpty())
    return;

  QJsonObject body;
  body["roomName"] = m_roomName;

  postRequest("/api/ai/clear", body, [this](bool success, const QJsonObject &) {
    if (success) {
      qDebug() << "[AIAssistant] AI 历史已清除";
    }
  });
}

// ==================== 录音 + 离线转录控制 ====================

void AIAssistant::startRecording() {
  if (m_isRecordingAudio) {
    qDebug() << "[AIAssistant] 已经在录音中，忽略重复调用";
    return;
  }

  if (m_roomName.isEmpty()) {
    setError("房间名不能为空，无法启动录音");
    return;
  }

  if (m_userName.isEmpty()) {
    setError("用户名不能为空，无法启动录音");
    return;
  }

  // 清空缓冲区，开始录音
  m_audioRecordBuffer.clear();
  m_audioRecordBuffer.reserve(16000 * 2 * 60 *
                              5); // 预分配 5 分钟容量 (16kHz, 16-bit, mono)
  m_recordingSeconds = 0;
  m_isRecordingAudio = true;
  m_recordingTimer->start(1000); // 每秒更新

  emit recordingAudioChanged();
  emit recordingDurationChanged();

  qDebug() << "[AIAssistant] 开始本地录音"
           << "room=" << m_roomName << "user=" << m_userName;
}

void AIAssistant::stopRecordingAndTranscribe() {
  if (!m_isRecordingAudio) {
    qDebug() << "[AIAssistant] 未在录音，忽略停止调用";
    return;
  }

  // 停止录音
  m_isRecordingAudio = false;
  m_recordingTimer->stop();
  emit recordingAudioChanged();

  int bufferSize = m_audioRecordBuffer.size();
  float durationSec = bufferSize / (16000.0f * 2); // 16kHz, 16-bit mono

  qDebug() << "[AIAssistant] 停止录音，缓冲区大小:" << bufferSize << "字节, 约"
           << QString::number(durationSec, 'f', 1) << "秒";

  if (bufferSize < 3200) // 小于 0.1 秒，可能没有有效内容
  {
    setError("录音时间太短，请至少录制 1 秒以上");
    m_audioRecordBuffer.clear();
    return;
  }

  // 标记为转录中
  m_isTranscribing = true;
  emit transcribingChanged();

  // 组装 WAV 文件
  QByteArray wavData = buildWavFile(m_audioRecordBuffer, 16000, 1, 16);
  m_audioRecordBuffer.clear(); // 释放内存

  qDebug() << "[AIAssistant] WAV 文件大小:" << wavData.size() << "字节 ("
           << QString::number(wavData.size() / 1024.0 / 1024.0, 'f', 2)
           << "MB)";

  // 转为 base64
  QString audioBase64 = wavData.toBase64();

  // 发送到服务端进行离线 ASR
  QJsonObject body;
  body["roomName"] = m_roomName;
  body["audioData"] = audioBase64;
  body["username"] = m_userName;

  // 使用更长的超时时间（离线 ASR 可能需要几分钟）
  QUrl url(m_serverUrl + "/api/ai/transcribe");
  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  request.setTransferTimeout(360000); // 6 分钟超时

  QByteArray postData = QJsonDocument(body).toJson(QJsonDocument::Compact);
  qDebug() << "[AIAssistant] 提交转录请求, payload 大小:"
           << (postData.size() / 1024.0 / 1024.0) << "MB";

  QNetworkReply *reply = m_networkManager->post(request, postData);

  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    reply->deleteLater();
    m_isTranscribing = false;
    emit transcribingChanged();

    if (reply->error() != QNetworkReply::NoError) {
      QString errMsg = "转录请求失败: " + reply->errorString();
      qWarning() << "[AIAssistant]" << errMsg;
      setError(errMsg);
      emit transcriptionFailed(errMsg);
      return;
    }

    QByteArray responseData = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    QJsonObject json = doc.object();

    if (json["success"].toBool(false)) {
      QString transcript = json["transcript"].toString();
      qDebug() << "[AIAssistant] 转录成功:" << transcript.left(200);

      // 保存到转录历史
      if (!transcript.isEmpty() && transcript != "（无语音内容）" &&
          transcript != "（无识别结果）") {
        TranscriptEntry entry;
        entry.time = QDateTime::currentDateTime().toString("HH:mm:ss");
        entry.speaker = m_userName;
        entry.text = transcript;
        m_transcripts.append(entry);

        emit transcriptCountChanged();
        emit newTranscriptReceived(transcript);
      }

      emit transcriptionCompleted(transcript);
    } else {
      QString errMsg = json["error"].toString("转录失败");
      qWarning() << "[AIAssistant] 转录失败:" << errMsg;
      setError(errMsg);
      emit transcriptionFailed(errMsg);
    }
  });
}

void AIAssistant::feedAudioData(const QByteArray &pcmData, int sampleRate,
                                int channels) {
  // 仅在录音状态时才缓存数据
  if (!m_isRecordingAudio)
    return;

  const int targetRate = 16000; // ASR 要求 16kHz
  QByteArray chunk;

  if (sampleRate == targetRate && channels == 1) {
    // 已经是 16kHz 单声道，直接使用
    chunk = pcmData;
  } else {
    // --- 带低通滤波的降采样 + 混音到单声道 ---
    const int16_t *src = reinterpret_cast<const int16_t *>(pcmData.constData());
    int totalSamples = pcmData.size() / sizeof(int16_t);
    int frames = totalSamples / channels;

    int ratio = sampleRate / targetRate;
    if (ratio < 1)
      ratio = 1;

    int outFrames = frames / ratio;
    chunk.resize(outFrames * static_cast<int>(sizeof(int16_t)));
    int16_t *dst = reinterpret_cast<int16_t *>(chunk.data());

    for (int i = 0; i < outFrames; ++i) {
      // 对 ratio 个输入帧取平均值（简单低通滤波，防止混叠）
      int64_t sum = 0;
      int count = 0;
      for (int j = 0; j < ratio; ++j) {
        int srcFrame = i * ratio + j;
        if (srcFrame >= frames)
          break;

        if (channels == 1) {
          sum += src[srcFrame];
        } else {
          // 多声道混音：取所有声道平均值
          int32_t chSum = 0;
          for (int ch = 0; ch < channels; ++ch)
            chSum += src[srcFrame * channels + ch];
          sum += chSum / channels;
        }
        count++;
      }
      dst[i] = static_cast<int16_t>(count > 0 ? sum / count : 0);
    }
  }

  // 追加到本地录音缓冲区
  m_audioRecordBuffer.append(chunk);
}

QVariantList AIAssistant::getTranscripts() const {
  QVariantList result;
  for (const auto &entry : m_transcripts) {
    QVariantMap map;
    map["time"] = entry.time;
    map["speaker"] = entry.speaker;
    map["text"] = entry.text;
    result.append(map);
  }
  return result;
}

void AIAssistant::clearTranscripts() {
  m_transcripts.clear();
  emit transcriptCountChanged();
}

// ==================== WAV 文件组装 ====================

QByteArray AIAssistant::buildWavFile(const QByteArray &pcmData, int sampleRate,
                                     int channels, int bitsPerSample) const {
  // WAV 文件头结构（44 字节）
  int dataSize = pcmData.size();
  int byteRate = sampleRate * channels * bitsPerSample / 8;
  int blockAlign = channels * bitsPerSample / 8;

  QByteArray wav;
  wav.reserve(44 + dataSize);

  // RIFF 头
  wav.append("RIFF", 4);
  int chunkSize = 36 + dataSize;
  wav.append(reinterpret_cast<const char *>(&chunkSize), 4);
  wav.append("WAVE", 4);

  // fmt 子块
  wav.append("fmt ", 4);
  int subchunk1Size = 16;
  wav.append(reinterpret_cast<const char *>(&subchunk1Size), 4);
  int16_t audioFormat = 1; // PCM
  wav.append(reinterpret_cast<const char *>(&audioFormat), 2);
  int16_t numChannels = static_cast<int16_t>(channels);
  wav.append(reinterpret_cast<const char *>(&numChannels), 2);
  wav.append(reinterpret_cast<const char *>(&sampleRate), 4);
  wav.append(reinterpret_cast<const char *>(&byteRate), 4);
  int16_t blockAlignShort = static_cast<int16_t>(blockAlign);
  wav.append(reinterpret_cast<const char *>(&blockAlignShort), 2);
  int16_t bps = static_cast<int16_t>(bitsPerSample);
  wav.append(reinterpret_cast<const char *>(&bps), 2);

  // data 子块
  wav.append("data", 4);
  wav.append(reinterpret_cast<const char *>(&dataSize), 4);
  wav.append(pcmData);

  return wav;
}

// ==================== HTTP 请求工具 ====================

void AIAssistant::postRequest(
    const QString &endpoint, const QJsonObject &body,
    std::function<void(bool, const QJsonObject &)> callback) {
  QUrl url(m_serverUrl + endpoint);
  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  request.setTransferTimeout(60000);

  QByteArray postData = QJsonDocument(body).toJson(QJsonDocument::Compact);
  QNetworkReply *reply = m_networkManager->post(request, postData);

  connect(reply, &QNetworkReply::finished, this, [reply, callback]() {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
      QJsonObject errObj;
      errObj["error"] = reply->errorString();
      callback(false, errObj);
      return;
    }

    QByteArray responseData = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    QJsonObject json = doc.object();

    bool success = json["success"].toBool(false);
    callback(success, json);
  });
}

// ==================== 工具方法 ====================

void AIAssistant::setError(const QString &error) {
  m_lastError = error;
  emit lastErrorChanged();
  emit aiError(error);
  qWarning() << "[AIAssistant] 错误:" << error;
}

void AIAssistant::setBusy(bool busy) {
  if (m_isBusy != busy) {
    m_isBusy = busy;
    emit busyChanged();
  }
}
