#include "aiassistant.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QNetworkReply>
#include <QDebug>

AIAssistant::AIAssistant(QObject *parent)
    : QObject(parent),
      m_networkManager(new QNetworkAccessManager(this)),
      m_serverUrl("http://8.162.3.195:3000"),
      m_isBusy(false),
      m_isTranscribing(false),
      m_transcriptionTimer(new QTimer(this)),
      m_audioSampleRate(48000),
      m_audioChannels(1)
{
    m_transcriptionTimer->setInterval(TRANSCRIPTION_INTERVAL_MS);
    connect(m_transcriptionTimer, &QTimer::timeout, this,
            &AIAssistant::onTranscriptionTimerTick);

    qDebug() << "[AIAssistant] 初始化完成";
}

AIAssistant::~AIAssistant()
{
    if (m_isTranscribing)
    {
        stopTranscription();
    }
}

// ==================== 属性 Getter ====================

bool AIAssistant::isBusy() const { return m_isBusy; }
QString AIAssistant::lastReply() const { return m_lastReply; }
QString AIAssistant::lastError() const { return m_lastError; }
bool AIAssistant::isTranscribing() const { return m_isTranscribing; }
QString AIAssistant::liveTranscript() const { return m_liveTranscript; }
int AIAssistant::transcriptCount() const { return m_transcripts.count(); }
QString AIAssistant::serverUrl() const { return m_serverUrl; }
QString AIAssistant::roomName() const { return m_roomName; }
QString AIAssistant::userName() const { return m_userName; }

// ==================== 属性 Setter ====================

void AIAssistant::setServerUrl(const QString &url)
{
    if (m_serverUrl != url)
    {
        m_serverUrl = url;
        emit serverUrlChanged();
    }
}

void AIAssistant::setRoomName(const QString &name)
{
    if (m_roomName != name)
    {
        m_roomName = name;
        emit roomNameChanged();
    }
}

void AIAssistant::setUserName(const QString &name)
{
    if (m_userName != name)
    {
        m_userName = name;
        emit userNameChanged();
    }
}

// ==================== AI 对话接口 ====================

void AIAssistant::sendMessage(const QString &message)
{
    if (message.trimmed().isEmpty() || m_roomName.isEmpty())
    {
        setError("消息或房间名不能为空");
        return;
    }

    setBusy(true);

    QJsonObject body;
    body["roomName"] = m_roomName;
    body["username"] = m_userName;
    body["message"] = message;

    postRequest("/api/ai/chat", body,
                [this](bool success, const QJsonObject &json)
                {
                    setBusy(false);
                    if (success)
                    {
                        m_lastReply = json["reply"].toString();
                        emit lastReplyChanged();
                        emit aiReplyReceived(m_lastReply);
                        qDebug() << "[AIAssistant] AI 回复:" << m_lastReply.left(80);
                    }
                    else
                    {
                        setError(json["error"].toString("AI 请求失败"));
                    }
                });
}

void AIAssistant::summarize(const QVariantList &chatHistory)
{
    if (m_roomName.isEmpty())
    {
        setError("房间名不能为空");
        return;
    }

    setBusy(true);

    QJsonArray historyArray;
    for (const auto &item : chatHistory)
    {
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
                [this](bool success, const QJsonObject &json)
                {
                    setBusy(false);
                    if (success)
                    {
                        QString summary = json["summary"].toString();
                        emit summaryReceived(summary);
                    }
                    else
                    {
                        setError(json["error"].toString("摘要生成失败"));
                    }
                });
}

void AIAssistant::generateMeetingMinutes(const QVariantList &chatHistory)
{
    if (m_roomName.isEmpty())
    {
        setError("房间名不能为空");
        return;
    }

    setBusy(true);

    // 构建转录数组
    QJsonArray transcriptsArray;
    for (const auto &entry : m_transcripts)
    {
        QJsonObject obj;
        obj["time"] = entry.time;
        obj["speaker"] = entry.speaker;
        obj["text"] = entry.text;
        transcriptsArray.append(obj);
    }

    // 构建聊天记录数组
    QJsonArray chatArray;
    for (const auto &item : chatHistory)
    {
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
                [this](bool success, const QJsonObject &json)
                {
                    setBusy(false);
                    if (success)
                    {
                        QString minutes = json["minutes"].toString();
                        emit meetingMinutesReceived(minutes);
                    }
                    else
                    {
                        setError(json["error"].toString("会议纪要生成失败"));
                    }
                });
}

void AIAssistant::clearAIHistory()
{
    if (m_roomName.isEmpty())
        return;

    QJsonObject body;
    body["roomName"] = m_roomName;

    postRequest("/api/ai/clear", body,
                [this](bool success, const QJsonObject &)
                {
                    if (success)
                    {
                        qDebug() << "[AIAssistant] AI 历史已清除";
                    }
                });
}

// ==================== 语音转录控制 ====================

void AIAssistant::startTranscription()
{
    if (m_isTranscribing)
        return;

    m_isTranscribing = true;
    emit transcribingChanged();

    // 清空缓冲区
    {
        QMutexLocker locker(&m_audioBufferMutex);
        m_audioBuffer.clear();
    }

    m_transcriptionTimer->start();
    qDebug() << "[AIAssistant] 语音转录已开始，间隔:"
             << TRANSCRIPTION_INTERVAL_MS / 1000 << "秒";
}

void AIAssistant::stopTranscription()
{
    if (!m_isTranscribing)
        return;

    m_transcriptionTimer->stop();

    // 发送剩余的缓冲数据
    onTranscriptionTimerTick();

    m_isTranscribing = false;
    emit transcribingChanged();
    qDebug() << "[AIAssistant] 语音转录已停止";
}

void AIAssistant::feedAudioData(const QByteArray &pcmData, int sampleRate,
                                int channels)
{
    if (!m_isTranscribing)
        return;

    QMutexLocker locker(&m_audioBufferMutex);
    m_audioSampleRate = sampleRate;
    m_audioChannels = channels;
    m_audioBuffer.append(pcmData);
}

QVariantList AIAssistant::getTranscripts() const
{
    QVariantList result;
    for (const auto &entry : m_transcripts)
    {
        QVariantMap map;
        map["time"] = entry.time;
        map["speaker"] = entry.speaker;
        map["text"] = entry.text;
        result.append(map);
    }
    return result;
}

void AIAssistant::clearTranscripts()
{
    m_transcripts.clear();
    m_liveTranscript.clear();
    emit liveTranscriptChanged();
    emit transcriptCountChanged();
}

// ==================== 定时转录 ====================

void AIAssistant::onTranscriptionTimerTick()
{
    QByteArray pcmData;
    int sampleRate, channels;

    {
        QMutexLocker locker(&m_audioBufferMutex);
        if (m_audioBuffer.size() < MIN_AUDIO_BYTES)
        {
            qDebug() << "[AIAssistant] 音频数据不足，跳过本次转录:"
                     << m_audioBuffer.size() << "bytes (需要" << MIN_AUDIO_BYTES
                     << ")";
            return;
        }
        pcmData = m_audioBuffer;
        sampleRate = m_audioSampleRate;
        channels = m_audioChannels;
        m_audioBuffer.clear();
    }

    // PCM → WAV
    QByteArray wavData = encodePcmToWav(pcmData, sampleRate, channels);
    QString base64Audio = wavData.toBase64();

    qDebug() << "[AIAssistant] 发送转录请求: PCM=" << pcmData.size()
             << "bytes WAV=" << wavData.size() << "bytes";

    QJsonObject body;
    body["roomName"] = m_roomName;
    body["audioData"] = base64Audio;
    body["mimeType"] = "audio/wav";

    postRequest("/api/ai/transcribe", body,
                [this](bool success, const QJsonObject &json)
                {
                    if (success)
                    {
                        QString text = json["transcript"].toString().trimmed();
                        if (!text.isEmpty())
                        {
                            TranscriptEntry entry;
                            entry.time =
                                QDateTime::currentDateTime().toString("HH:mm:ss");
                            entry.speaker = m_userName;
                            entry.text = text;
                            m_transcripts.append(entry);

                            m_liveTranscript = text;
                            emit liveTranscriptChanged();
                            emit transcriptCountChanged();
                            emit newTranscriptReceived(text);

                            qDebug() << "[AIAssistant] 转录结果:" << text.left(100);
                        }
                    }
                    else
                    {
                        qWarning() << "[AIAssistant] 转录失败:"
                                   << json["error"].toString();
                    }
                });
}

// ==================== WAV 编码 ====================

QByteArray AIAssistant::encodePcmToWav(const QByteArray &pcmData,
                                       int sampleRate, int channels,
                                       int bitsPerSample)
{
    int dataSize = pcmData.size();
    int byteRate = sampleRate * channels * bitsPerSample / 8;
    int blockAlign = channels * bitsPerSample / 8;

    QByteArray wav;
    QBuffer buffer(&wav);
    buffer.open(QIODevice::WriteOnly);

    // RIFF header
    buffer.write("RIFF", 4);
    qint32 chunkSize = 36 + dataSize;
    buffer.write(reinterpret_cast<const char *>(&chunkSize), 4);
    buffer.write("WAVE", 4);

    // fmt sub-chunk
    buffer.write("fmt ", 4);
    qint32 subchunk1Size = 16;
    buffer.write(reinterpret_cast<const char *>(&subchunk1Size), 4);
    qint16 audioFormat = 1; // PCM
    buffer.write(reinterpret_cast<const char *>(&audioFormat), 2);
    qint16 numChannels = static_cast<qint16>(channels);
    buffer.write(reinterpret_cast<const char *>(&numChannels), 2);
    qint32 sRate = sampleRate;
    buffer.write(reinterpret_cast<const char *>(&sRate), 4);
    qint32 bRate = byteRate;
    buffer.write(reinterpret_cast<const char *>(&bRate), 4);
    qint16 bAlign = static_cast<qint16>(blockAlign);
    buffer.write(reinterpret_cast<const char *>(&bAlign), 2);
    qint16 bps = static_cast<qint16>(bitsPerSample);
    buffer.write(reinterpret_cast<const char *>(&bps), 2);

    // data sub-chunk
    buffer.write("data", 4);
    qint32 dSize = dataSize;
    buffer.write(reinterpret_cast<const char *>(&dSize), 4);
    buffer.write(pcmData);

    buffer.close();
    return wav;
}

// ==================== HTTP 请求工具 ====================

void AIAssistant::postRequest(
    const QString &endpoint, const QJsonObject &body,
    std::function<void(bool, const QJsonObject &)> callback)
{
    QUrl url(m_serverUrl + endpoint);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    // 语音转录数据可能很大，设置较长超时
    request.setTransferTimeout(60000);

    QByteArray postData = QJsonDocument(body).toJson(QJsonDocument::Compact);
    QNetworkReply *reply = m_networkManager->post(request, postData);

    connect(reply, &QNetworkReply::finished, this, [reply, callback]()
            {
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
    callback(success, json); });
}

// ==================== 工具方法 ====================

void AIAssistant::setError(const QString &error)
{
    m_lastError = error;
    emit lastErrorChanged();
    emit aiError(error);
    qWarning() << "[AIAssistant] 错误:" << error;
}

void AIAssistant::setBusy(bool busy)
{
    if (m_isBusy != busy)
    {
        m_isBusy = busy;
        emit busyChanged();
    }
}
