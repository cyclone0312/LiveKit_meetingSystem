/**
 * @file screencapture.h
 * @brief 屏幕捕获管理器
 *
 * 负责：
 * 1. Windows DXGI Desktop Duplication 屏幕捕获
 * 2. 将捕获的帧转换为 LiveKit SDK 格式
 * 3. 提供 QML 可用的屏幕列表
 */

#ifndef SCREENCAPTURE_H
#define SCREENCAPTURE_H

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QTimer>
#include <QVideoSink>
#include <atomic>
#include <memory>

// Windows headers for DXGI
#ifdef Q_OS_WIN
#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#endif

// LiveKit SDK
#include <livekit/local_video_track.h>
#include <livekit/video_frame.h>
#include <livekit/video_source.h>

/**
 * @brief 屏幕信息结构
 */
struct ScreenInfo {
  int index;
  QString name;
  int width;
  int height;
  bool isPrimary;
};

/**
 * @brief 屏幕捕获管理器
 *
 * 使用 Windows DXGI Desktop Duplication API 捕获屏幕内容
 */
class ScreenCapture : public QObject {
  Q_OBJECT

  Q_PROPERTY(bool isActive READ isActive NOTIFY activeChanged)
  Q_PROPERTY(
      QStringList availableScreens READ availableScreens NOTIFY screensChanged)
  Q_PROPERTY(int currentScreenIndex READ currentScreenIndex WRITE
                 setCurrentScreenIndex NOTIFY currentScreenIndexChanged)
  Q_PROPERTY(QVideoSink *videoSink READ videoSink WRITE setVideoSink NOTIFY
                 videoSinkChanged)

public:
  explicit ScreenCapture(QObject *parent = nullptr);
  ~ScreenCapture();

  // 属性 Getter
  bool isActive() const { return m_isActive; }
  QStringList availableScreens() const;
  int currentScreenIndex() const { return m_currentScreenIndex; }
  QVideoSink *videoSink() const { return m_externalVideoSink; }

  // 属性 Setter
  void setCurrentScreenIndex(int index);
  void setVideoSink(QVideoSink *sink);

  // QML 可调用的方法
  Q_INVOKABLE void bindVideoSink(QVideoSink *sink) { setVideoSink(sink); }

  // 获取 LiveKit 轨道
  std::shared_ptr<livekit::LocalVideoTrack> getScreenTrack();

  // 重置 LiveKit 源和轨道（离开房间后调用）
  void resetLiveKitSources();

public slots:
  /**
   * @brief 启动屏幕捕获
   * @param screenIndex 要捕获的屏幕索引，-1 表示使用当前选中的屏幕
   */
  void startCapture(int screenIndex = -1);

  /**
   * @brief 停止屏幕捕获
   */
  void stopCapture();

  /**
   * @brief 刷新屏幕列表
   */
  void refreshScreens();

signals:
  void activeChanged();
  void screensChanged();
  void currentScreenIndexChanged();
  void videoSinkChanged();
  void captureError(const QString &error);
  void frameCaptured();

private slots:
  void onCaptureTimer();

private:
  bool initializeDXGI(int screenIndex);
  void cleanupDXGI();
  bool captureFrame();

private:
  // 状态
  std::atomic<bool> m_isActive{false};
  int m_currentScreenIndex = 0;
  QList<ScreenInfo> m_screens;

  // 定时器
  std::unique_ptr<QTimer> m_captureTimer;
  static const int CAPTURE_FPS = 30;

  // LiveKit 源和轨道
  std::shared_ptr<livekit::VideoSource> m_screenSource;
  std::shared_ptr<livekit::LocalVideoTrack> m_screenTrack;

  // 视频参数
  int m_captureWidth = 1920;
  int m_captureHeight = 1080;

  // 帧计数（用于日志）
  int m_frameCount = 0;

  // 本地预览 VideoSink
  QPointer<QVideoSink> m_externalVideoSink;

#ifdef Q_OS_WIN
  // Windows DXGI 资源
  ComPtr<ID3D11Device> m_d3dDevice;
  ComPtr<ID3D11DeviceContext> m_d3dContext;
  ComPtr<IDXGIOutputDuplication> m_deskDupl;
  ComPtr<ID3D11Texture2D> m_stagingTexture;
  DXGI_OUTPUT_DESC m_outputDesc;
#endif
};

#endif // SCREENCAPTURE_H
