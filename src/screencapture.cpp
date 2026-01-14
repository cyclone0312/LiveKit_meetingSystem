/**
 * @file screencapture.cpp
 * @brief 屏幕捕获管理器实现
 */

#include "screencapture.h"
#include <QDebug>
#include <QGuiApplication>
#include <QImage>
#include <QScreen>
#include <QThread>
#include <QVideoFrame>
#include <chrono>

// =============================================================================
// ScreenCapture 实现
// =============================================================================

ScreenCapture::ScreenCapture(QObject *parent)
    : QObject(parent), m_captureTimer(std::make_unique<QTimer>(this)) {
  qDebug() << "[ScreenCapture] 初始化中...";

  // 刷新屏幕列表
  refreshScreens();

  // 创建 LiveKit 源
  m_screenSource =
      std::make_shared<livekit::VideoSource>(m_captureWidth, m_captureHeight);
  m_screenTrack =
      livekit::LocalVideoTrack::createLocalVideoTrack("screen", m_screenSource);

  // 连接定时器信号
  connect(m_captureTimer.get(), &QTimer::timeout, this,
          &ScreenCapture::onCaptureTimer);
  m_captureTimer->setTimerType(Qt::PreciseTimer);

  qDebug() << "[ScreenCapture] 初始化完成";
}

ScreenCapture::~ScreenCapture() {
  stopCapture();
  qDebug() << "[ScreenCapture] 已销毁";
}

void ScreenCapture::refreshScreens() {
  m_screens.clear();

#ifdef Q_OS_WIN
  // 枚举显示器
  int index = 0;
  EnumDisplayMonitors(
      nullptr, nullptr,
      [](HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam) -> BOOL {
        auto screens = reinterpret_cast<QList<ScreenInfo> *>(lParam);
        MONITORINFOEX info;
        info.cbSize = sizeof(MONITORINFOEX);
        if (GetMonitorInfo(hMonitor, &info)) {
          ScreenInfo screen;
          screen.index = screens->size();
          screen.name = QString("显示器 %1").arg(screens->size() + 1);
          screen.width = info.rcMonitor.right - info.rcMonitor.left;
          screen.height = info.rcMonitor.bottom - info.rcMonitor.top;
          screen.isPrimary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
          if (screen.isPrimary) {
            screen.name += " (主屏幕)";
          }
          screens->append(screen);
        }
        return TRUE;
      },
      reinterpret_cast<LPARAM>(&m_screens));
#else
  // 非 Windows 平台使用 Qt 屏幕信息
  const auto screens = QGuiApplication::screens();
  for (int i = 0; i < screens.size(); ++i) {
    ScreenInfo info;
    info.index = i;
    info.name = screens[i]->name();
    info.width = screens[i]->size().width();
    info.height = screens[i]->size().height();
    info.isPrimary = (screens[i] == QGuiApplication::primaryScreen());
    m_screens.append(info);
  }
#endif

  qDebug() << "[ScreenCapture] 发现" << m_screens.count() << "个屏幕:";
  for (const auto &screen : m_screens) {
    qDebug() << "  -" << screen.name << screen.width << "x" << screen.height;
  }

  emit screensChanged();
}

QStringList ScreenCapture::availableScreens() const {
  QStringList list;
  for (const auto &screen : m_screens) {
    list.append(QString("%1 (%2x%3)")
                    .arg(screen.name)
                    .arg(screen.width)
                    .arg(screen.height));
  }
  return list;
}

void ScreenCapture::setCurrentScreenIndex(int index) {
  if (index >= 0 && index < m_screens.count() &&
      m_currentScreenIndex != index) {
    m_currentScreenIndex = index;

    // 如果正在捕获，重新启动以使用新屏幕
    if (m_isActive) {
      stopCapture();
      startCapture(index);
    }

    emit currentScreenIndexChanged();
  }
}

void ScreenCapture::setVideoSink(QVideoSink *sink) {
  if (m_externalVideoSink != sink) {
    m_externalVideoSink = sink;
    emit videoSinkChanged();
    qDebug() << "[ScreenCapture] 本地预览 VideoSink 已设置:"
             << (sink ? "有效" : "null");
  }
}

std::shared_ptr<livekit::LocalVideoTrack> ScreenCapture::getScreenTrack() {
  return m_screenTrack;
}

void ScreenCapture::resetLiveKitSources() {
  qDebug() << "[ScreenCapture] 重置 LiveKit 源和轨道...";

  // 停止捕获
  stopCapture();

  // 清空旧的轨道和源
  m_screenTrack.reset();
  m_screenSource.reset();

  // 创建新的源和轨道
  m_screenSource =
      std::make_shared<livekit::VideoSource>(m_captureWidth, m_captureHeight);
  m_screenTrack =
      livekit::LocalVideoTrack::createLocalVideoTrack("screen", m_screenSource);

  qDebug() << "[ScreenCapture] LiveKit 源和轨道重置完成";
}

void ScreenCapture::startCapture(int screenIndex) {
  if (m_isActive) {
    qDebug() << "[ScreenCapture] 已在捕获中";
    return;
  }

  int targetScreen = (screenIndex >= 0) ? screenIndex : m_currentScreenIndex;

  if (targetScreen < 0 || targetScreen >= m_screens.count()) {
    qWarning() << "[ScreenCapture] 无效的屏幕索引:" << targetScreen;
    emit captureError("无效的屏幕索引");
    return;
  }

  qDebug() << "[ScreenCapture] 启动屏幕捕获, 屏幕:" << targetScreen;

#ifdef Q_OS_WIN
  if (!initializeDXGI(targetScreen)) {
    qWarning() << "[ScreenCapture] DXGI 初始化失败";
    emit captureError("无法初始化屏幕捕获");
    return;
  }
#endif

  m_currentScreenIndex = targetScreen;
  m_isActive = true;
  m_frameCount = 0;

  // 启动定时器（30 FPS）
  m_captureTimer->start(1000 / CAPTURE_FPS);

  emit activeChanged();
  qDebug() << "[ScreenCapture] 屏幕捕获已启动, FPS:" << CAPTURE_FPS;
}

void ScreenCapture::stopCapture() {
  if (!m_isActive) {
    return;
  }

  qDebug() << "[ScreenCapture] 停止屏幕捕获...";

  // 首先停止定时器，防止新的帧捕获
  m_captureTimer->stop();
  m_isActive = false;

  // 【关键】清除外部 VideoSink 引用，防止访问已销毁的 QML 对象
  m_externalVideoSink.clear();

  // 【关键】等待一小段时间，确保正在进行的帧捕获完成
  // 这是必要的，因为定时器回调可能正在执行中
  QThread::msleep(100);

#ifdef Q_OS_WIN
  cleanupDXGI();
#endif

  emit activeChanged();
  qDebug() << "[ScreenCapture] 屏幕捕获已停止, 共捕获" << m_frameCount << "帧";
}

void ScreenCapture::onCaptureTimer() {
  if (!m_isActive) {
    return;
  }

  captureFrame();
}

#ifdef Q_OS_WIN

bool ScreenCapture::initializeDXGI(int screenIndex) {
  qDebug() << "[ScreenCapture] 初始化 DXGI, 屏幕:" << screenIndex;

  HRESULT hr;

  // 创建 D3D11 设备
  D3D_FEATURE_LEVEL featureLevels[] = {
      D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
      D3D_FEATURE_LEVEL_10_0};

  hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                         featureLevels, ARRAYSIZE(featureLevels),
                         D3D11_SDK_VERSION, &m_d3dDevice, nullptr,
                         &m_d3dContext);

  if (FAILED(hr)) {
    qWarning() << "[ScreenCapture] D3D11CreateDevice 失败:"
               << QString::number(hr, 16);
    return false;
  }

  // 获取 DXGI 设备
  ComPtr<IDXGIDevice> dxgiDevice;
  hr = m_d3dDevice.As(&dxgiDevice);
  if (FAILED(hr)) {
    qWarning() << "[ScreenCapture] 获取 DXGI 设备失败";
    return false;
  }

  // 获取 DXGI 适配器
  ComPtr<IDXGIAdapter> dxgiAdapter;
  hr = dxgiDevice->GetAdapter(&dxgiAdapter);
  if (FAILED(hr)) {
    qWarning() << "[ScreenCapture] 获取 DXGI 适配器失败";
    return false;
  }

  // 枚举输出（显示器）
  ComPtr<IDXGIOutput> dxgiOutput;
  hr = dxgiAdapter->EnumOutputs(screenIndex, &dxgiOutput);
  if (FAILED(hr)) {
    qWarning() << "[ScreenCapture] 枚举输出失败, 屏幕:" << screenIndex;
    return false;
  }

  // 获取输出描述
  hr = dxgiOutput->GetDesc(&m_outputDesc);
  if (FAILED(hr)) {
    qWarning() << "[ScreenCapture] 获取输出描述失败";
    return false;
  }

  m_captureWidth = m_outputDesc.DesktopCoordinates.right -
                   m_outputDesc.DesktopCoordinates.left;
  m_captureHeight = m_outputDesc.DesktopCoordinates.bottom -
                    m_outputDesc.DesktopCoordinates.top;
  qDebug() << "[ScreenCapture] 屏幕尺寸:" << m_captureWidth << "x"
           << m_captureHeight;

  // 更新 VideoSource 尺寸
  m_screenSource.reset();
  m_screenSource =
      std::make_shared<livekit::VideoSource>(m_captureWidth, m_captureHeight);
  m_screenTrack =
      livekit::LocalVideoTrack::createLocalVideoTrack("screen", m_screenSource);

  // 获取 IDXGIOutput1 接口
  ComPtr<IDXGIOutput1> dxgiOutput1;
  hr = dxgiOutput.As(&dxgiOutput1);
  if (FAILED(hr)) {
    qWarning() << "[ScreenCapture] 获取 IDXGIOutput1 失败";
    return false;
  }

  // 创建桌面复制
  hr = dxgiOutput1->DuplicateOutput(m_d3dDevice.Get(), &m_deskDupl);
  if (FAILED(hr)) {
    qWarning() << "[ScreenCapture] DuplicateOutput 失败:"
               << QString::number(hr, 16);
    if (hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE) {
      qWarning() << "[ScreenCapture] 桌面复制不可用（可能已被其他应用占用）";
    }
    return false;
  }

  // 创建 staging 纹理用于 CPU 读取
  D3D11_TEXTURE2D_DESC texDesc = {};
  texDesc.Width = m_captureWidth;
  texDesc.Height = m_captureHeight;
  texDesc.MipLevels = 1;
  texDesc.ArraySize = 1;
  texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  texDesc.SampleDesc.Count = 1;
  texDesc.Usage = D3D11_USAGE_STAGING;
  texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

  hr = m_d3dDevice->CreateTexture2D(&texDesc, nullptr, &m_stagingTexture);
  if (FAILED(hr)) {
    qWarning() << "[ScreenCapture] 创建 staging 纹理失败";
    return false;
  }

  qDebug() << "[ScreenCapture] DXGI 初始化成功";
  return true;
}

void ScreenCapture::cleanupDXGI() {
  qDebug() << "[ScreenCapture] 清理 DXGI 资源...";

  m_stagingTexture.Reset();
  m_deskDupl.Reset();
  m_d3dContext.Reset();
  m_d3dDevice.Reset();

  qDebug() << "[ScreenCapture] DXGI 资源已清理";
}

bool ScreenCapture::captureFrame() {
  if (!m_deskDupl || !m_screenSource) {
    return false;
  }

  HRESULT hr;
  DXGI_OUTDUPL_FRAME_INFO frameInfo;
  ComPtr<IDXGIResource> desktopResource;

  // 尝试获取下一帧
  hr = m_deskDupl->AcquireNextFrame(0, &frameInfo, &desktopResource);

  if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
    // 没有新帧（屏幕内容没变化），不算错误
    return true;
  }

  if (FAILED(hr)) {
    if (hr == DXGI_ERROR_ACCESS_LOST) {
      qWarning() << "[ScreenCapture] 访问丢失，需要重新初始化";
      // 清理并标记需要重新初始化
      cleanupDXGI();
      m_isActive = false;
      emit captureError("屏幕访问丢失");
    }
    return false;
  }

  // 获取桌面纹理
  ComPtr<ID3D11Texture2D> desktopTexture;
  hr = desktopResource.As(&desktopTexture);
  if (FAILED(hr)) {
    m_deskDupl->ReleaseFrame();
    return false;
  }

  // 复制到 staging 纹理
  m_d3dContext->CopyResource(m_stagingTexture.Get(), desktopTexture.Get());

  // 映射 staging 纹理以读取像素数据
  D3D11_MAPPED_SUBRESOURCE mapped;
  hr = m_d3dContext->Map(m_stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
  if (FAILED(hr)) {
    m_deskDupl->ReleaseFrame();
    return false;
  }

  // 创建 LiveKit 视频帧（BGRA 格式）
  livekit::VideoFrame lkFrame = livekit::VideoFrame::create(
      m_captureWidth, m_captureHeight, livekit::VideoBufferType::BGRA);

  // 复制像素数据（注意行对齐）
  uint8_t *dstData = lkFrame.data();
  const uint8_t *srcData = static_cast<const uint8_t *>(mapped.pData);
  size_t dstRowBytes = m_captureWidth * 4;

  for (int y = 0; y < m_captureHeight; ++y) {
    std::memcpy(dstData + y * dstRowBytes, srcData + y * mapped.RowPitch,
                dstRowBytes);
  }

  // 取消映射
  m_d3dContext->Unmap(m_stagingTexture.Get(), 0);

  // 释放帧
  m_deskDupl->ReleaseFrame();

  // 获取时间戳
  auto now = std::chrono::steady_clock::now();
  auto timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                          now.time_since_epoch())
                          .count();

  try {
    // 发送到 LiveKit
    m_screenSource->captureFrame(lkFrame, timestamp_us);
    m_frameCount++;

    // 【关键】发送到本地预览 VideoSink
    QVideoSink *localSink = m_externalVideoSink.data();
    if (localSink) {
      // 从原始数据创建 QImage（BGRA 格式，与 DXGI 输出一致）
      // 【修复】必须使用 copy() 复制数据，否则 lkFrame 销毁后 QImage
      // 数据无效导致崩溃
      QImage image(lkFrame.data(), m_captureWidth, m_captureHeight,
                   m_captureWidth * 4, QImage::Format_ARGB32);
      QImage imageCopy = image.copy(); // 创建深拷贝
      QVideoFrame videoFrame(imageCopy);
      localSink->setVideoFrame(videoFrame);
    }

    // 每100帧打印一次日志
    if (m_frameCount % 100 == 0) {
      qDebug() << "[ScreenCapture] 已捕获" << m_frameCount << "帧";
    }

    emit frameCaptured();
  } catch (const std::exception &e) {
    if (m_frameCount % 100 == 0) {
      qWarning() << "[ScreenCapture] captureFrame 异常:" << e.what();
    }
  }

  return true;
}

#else // 非 Windows 平台

bool ScreenCapture::initializeDXGI(int screenIndex) {
  Q_UNUSED(screenIndex)
  qWarning() << "[ScreenCapture] DXGI 仅支持 Windows 平台";
  emit captureError("屏幕共享仅支持 Windows 系统");
  return false;
}

void ScreenCapture::cleanupDXGI() {
  // 非 Windows 平台无需清理
}

bool ScreenCapture::captureFrame() {
  // 非 Windows 平台暂不支持
  return false;
}

#endif // Q_OS_WIN
