/**
 * @file mediacapture.h
 * @brief 媒体采集管理器
 *
 * 负责：
 * 1. 摄像头视频采集
 * 2. 麦克风音频采集
 * 3. 将采集的帧转换为 LiveKit SDK 格式
 * 4. 提供 QML 可用的视频预览
 */

#ifndef MEDIACAPTURE_H
#define MEDIACAPTURE_H

#include <QObject>
#include <QCamera>
#include <QMediaCaptureSession>
#include <QVideoSink>
#include <QVideoFrame>
#include <QAudioSource>
#include <QAudioFormat>
#include <QIODevice>
#include <QMediaDevices>
#include <QCameraDevice>
#include <QAudioDevice>
#include <QImage>
#include <memory>

// LiveKit SDK
#include <livekit/video_source.h>
#include <livekit/video_frame.h>
#include <livekit/audio_source.h>
#include <livekit/audio_frame.h>
#include <livekit/local_video_track.h>
#include <livekit/local_audio_track.h>

// 前向声明
class MediaCapture;

/**
 * @brief 视频帧接收器
 * 用于接收摄像头帧并转发给 LiveKit
 */
class VideoFrameHandler : public QObject
{
    Q_OBJECT
public:
    explicit VideoFrameHandler(QObject *parent = nullptr);

    void setVideoSource(std::shared_ptr<livekit::VideoSource> source);
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

public slots:
    void handleVideoFrame(const QVideoFrame &frame);

signals:
    void frameProcessed();

private:
    std::shared_ptr<livekit::VideoSource> m_videoSource;
    bool m_enabled = false;
    int m_frameCount = 0;
};

/**
 * @brief 音频帧接收器
 * 用于接收麦克风数据并转发给 LiveKit
 */
class AudioFrameHandler : public QIODevice
{
    Q_OBJECT
public:
    explicit AudioFrameHandler(int sampleRate, int channels, QObject *parent = nullptr);

    void setAudioSource(std::shared_ptr<livekit::AudioSource> source);
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    // QIODevice 接口
    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;

private:
    std::shared_ptr<livekit::AudioSource> m_audioSource;
    bool m_enabled = false;
    int m_sampleRate;
    int m_numChannels;
};

/**
 * @brief 媒体采集管理器
 *
 * 管理摄像头和麦克风的采集，并提供给 LiveKit SDK 使用
 */
class MediaCapture : public QObject
{
    Q_OBJECT

    // QML 可访问的属性
    Q_PROPERTY(bool cameraActive READ isCameraActive NOTIFY cameraActiveChanged)
    Q_PROPERTY(bool microphoneActive READ isMicrophoneActive NOTIFY microphoneActiveChanged)
    Q_PROPERTY(QVideoSink *videoSink READ videoSink WRITE setVideoSink NOTIFY videoSinkChanged)
    Q_PROPERTY(QStringList availableCameras READ availableCameras NOTIFY availableCamerasChanged)
    Q_PROPERTY(QStringList availableMicrophones READ availableMicrophones NOTIFY availableMicrophonesChanged)
    Q_PROPERTY(int currentCameraIndex READ currentCameraIndex WRITE setCurrentCameraIndex NOTIFY currentCameraIndexChanged)
    Q_PROPERTY(int currentMicrophoneIndex READ currentMicrophoneIndex WRITE setCurrentMicrophoneIndex NOTIFY currentMicrophoneIndexChanged)

public:
    explicit MediaCapture(QObject *parent = nullptr);
    ~MediaCapture();

    // 属性 Getter
    bool isCameraActive() const;
    bool isMicrophoneActive() const;
    QVideoSink *videoSink() const;
    QStringList availableCameras() const;
    QStringList availableMicrophones() const;
    int currentCameraIndex() const;
    int currentMicrophoneIndex() const;

    // 属性 Setter
    void setVideoSink(QVideoSink *sink);
    void setCurrentCameraIndex(int index);
    void setCurrentMicrophoneIndex(int index);

    // 获取 LiveKit 轨道
    std::shared_ptr<livekit::LocalVideoTrack> getVideoTrack();
    std::shared_ptr<livekit::LocalAudioTrack> getAudioTrack();

    // 获取 LiveKit Source（用于直接访问）
    std::shared_ptr<livekit::VideoSource> getVideoSource() { return m_lkVideoSource; }
    std::shared_ptr<livekit::AudioSource> getAudioSource() { return m_lkAudioSource; }

public slots:
    /**
     * @brief 启动/停止摄像头
     */
    void startCamera();
    void stopCamera();
    void toggleCamera();

    /**
     * @brief 启动/停止麦克风
     */
    void startMicrophone();
    void stopMicrophone();
    void toggleMicrophone();

    /**
     * @brief 刷新设备列表
     */
    void refreshDevices();

signals:
    void cameraActiveChanged();
    void microphoneActiveChanged();
    void videoSinkChanged();
    void availableCamerasChanged();
    void availableMicrophonesChanged();
    void currentCameraIndexChanged();
    void currentMicrophoneIndexChanged();

    // 事件信号
    void cameraError(const QString &error);
    void microphoneError(const QString &error);
    void videoFrameCaptured();
    void audioFrameCaptured();

private slots:
    void onCameraActiveChanged(bool active);
    void onCameraErrorOccurred(QCamera::Error error, const QString &errorString);
    void onVideoFrameReceived(const QVideoFrame &frame);

private:
    void setupCamera();
    void setupMicrophone();
    void createLiveKitSources();

private:
    // Qt 媒体组件
    std::unique_ptr<QCamera> m_camera;
    std::unique_ptr<QMediaCaptureSession> m_captureSession;
    std::unique_ptr<QVideoSink> m_internalVideoSink;
    QVideoSink *m_externalVideoSink = nullptr;

    std::unique_ptr<QAudioSource> m_audioInput;
    std::unique_ptr<AudioFrameHandler> m_audioHandler;

    // 视频帧处理
    std::unique_ptr<VideoFrameHandler> m_videoHandler;

    // LiveKit 源和轨道 (使用 shared_ptr 因为 SDK API 需要)
    std::shared_ptr<livekit::VideoSource> m_lkVideoSource;
    std::shared_ptr<livekit::AudioSource> m_lkAudioSource;
    std::shared_ptr<livekit::LocalVideoTrack> m_lkVideoTrack;
    std::shared_ptr<livekit::LocalAudioTrack> m_lkAudioTrack;

    // 设备列表
    QList<QCameraDevice> m_cameraDevices;
    QList<QAudioDevice> m_microphoneDevices;
    int m_currentCameraIndex = 0;
    int m_currentMicrophoneIndex = 0;

    // 状态
    bool m_cameraActive = false;
    bool m_microphoneActive = false;

    // 视频参数
    static const int VIDEO_WIDTH = 1280;
    static const int VIDEO_HEIGHT = 720;

    // 音频参数
    static const int AUDIO_SAMPLE_RATE = 48000;
    static const int AUDIO_CHANNELS = 1;
};

#endif // MEDIACAPTURE_H
