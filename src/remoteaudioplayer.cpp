/**
 * @file remoteaudioplayer.cpp
 * @brief 远程音频播放器实现
 */

#include "remoteaudioplayer.h"
#include <QDebug>
#include <QMediaDevices>
#include <QAudioDevice>
#include <thread>

RemoteAudioPlayer::RemoteAudioPlayer(std::shared_ptr<livekit::Track> audioTrack,
                                       QObject *parent)
    : QObject(parent)
    , m_track(audioTrack)
{
    qDebug() << "[RemoteAudioPlayer] 创建远程音频播放器";
}

RemoteAudioPlayer::~RemoteAudioPlayer()
{
    stop();
    qDebug() << "[RemoteAudioPlayer] 销毁远程音频播放器";
}

void RemoteAudioPlayer::start()
{
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
    options.capacity = 10;  // 缓冲 10 帧

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

    // 在后台线程中运行播放循环
    std::thread([this]() {
        playbackLoop();
    }).detach();
}

void RemoteAudioPlayer::stop()
{
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

    // 停止音频输出
    QMutexLocker locker(&m_mutex);
    if (m_audioSink) {
        m_audioSink->stop();
        m_audioSink.reset();
        m_audioDevice = nullptr;
    }
}

void RemoteAudioPlayer::setVolume(float volume)
{
    m_volume = qBound(0.0f, volume, 1.0f);
    QMutexLocker locker(&m_mutex);
    if (m_audioSink) {
        m_audioSink->setVolume(m_volume);
    }
}

bool RemoteAudioPlayer::initAudioOutput(int sampleRate, int channels)
{
    QMutexLocker locker(&m_mutex);

    // 设置音频格式
    QAudioFormat format;
    format.setSampleRate(sampleRate);
    format.setChannelCount(channels);
    format.setSampleFormat(QAudioFormat::Int16);  // LiveKit 使用 int16 PCM

    // 获取默认输出设备
    QAudioDevice outputDevice = QMediaDevices::defaultAudioOutput();
    if (outputDevice.isNull()) {
        qWarning() << "[RemoteAudioPlayer] 没有可用的音频输出设备";
        return false;
    }

    // 检查格式是否支持
    if (!outputDevice.isFormatSupported(format)) {
        qWarning() << "[RemoteAudioPlayer] 音频格式不支持，尝试最接近的格式";
        // 尝试使用最接近的格式
    }

    // 创建音频输出
    m_audioSink = std::make_unique<QAudioSink>(outputDevice, format);
    m_audioSink->setVolume(m_volume);

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
             << "采样率=" << sampleRate
             << "声道=" << channels;

    return true;
}

void RemoteAudioPlayer::playbackLoop()
{
    qDebug() << "[RemoteAudioPlayer] 播放循环开始";

    bool audioInitialized = false;

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

        const livekit::AudioFrame &frame = event.frame;

        // 首次收到帧时初始化音频输出
        if (!audioInitialized) {
            if (!initAudioOutput(frame.sample_rate(), frame.num_channels())) {
                qWarning() << "[RemoteAudioPlayer] 音频输出初始化失败";
                break;
            }
            audioInitialized = true;
        }

        // 写入音频数据
        const std::vector<int16_t> &samples = frame.data();
        if (!samples.empty()) {
            QMutexLocker locker(&m_mutex);
            if (m_audioDevice && m_audioSink) {
                // 将 int16 数据转换为字节
                const char *data = reinterpret_cast<const char *>(samples.data());
                qint64 dataSize = static_cast<qint64>(samples.size() * sizeof(int16_t));

                // 写入音频设备
                qint64 written = m_audioDevice->write(data, dataSize);
                if (written < dataSize) {
                    qDebug() << "[RemoteAudioPlayer] 部分写入:" << written << "/" << dataSize;
                }
            }
        }
    }

    qDebug() << "[RemoteAudioPlayer] 播放循环结束";
}
