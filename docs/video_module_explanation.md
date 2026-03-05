# 会议视频模块核心原理解析

本篇文档将为您梳理基于 LiveKit 的会议系统中，视频模块从**摄像头采集** -> **SDK发送传输** -> **远端接收与渲染** 的完整技术链路、核心代码逻辑及关键参数作用。

---

## 1. 视频采集核心 (MediaCapture)

视频采集主要由 `MediaCapture` 和 `VideoFrameHandler` 类负责实现，位于 `src/mediacapture.h` 和 `src/mediacapture.cpp` 中。

### 1.1 核心链路
1. **获取设备与格式设置**：`MediaCapture::startCamera()` 加载系统摄像头设备，并优先尝试设置 `640x480` (@ >= 25fps) 的采集格式。
2. **底层管线搭建**：将 `QCamera` 传递给 `QMediaCaptureSession`，并将视频输出（Sink）挂载到内部隐藏的 `QVideoSink` (m_internalVideoSink) 上。
3. **拦截视频帧**：摄像头产生图像后进入 `m_internalVideoSink`，发射 `videoFrameChanged` 信号，从而触发并调用到核心处理函数：`VideoFrameHandler::handleVideoFrame(const QVideoFrame &frame)`。

### 1.2 帧处理逻辑与关键优化 (`VideoFrameHandler::handleVideoFrame`)
该方法的作用是将 Qt 的摄像头数据 `QVideoFrame` 转变为 LiveKit SDK 使用的 `livekit::VideoFrame`。
为了解决性能瓶颈、降低 CPU 占用及延迟，这里做了若干重要处理：

- **帧率控制**：
  通过 `std::chrono::steady_clock` 记录上一帧的时间。通过 `elapsed < 40000` (即 40ms) 的限制，主动将采集的帧率压制在 <= 25fps。这避免了将无用高帧率硬塞给 WebRTC 及服务端带来的带宽和 CPU 负荷。
- **内存拷贝与格式分流 (关键优化)**：
  - **BGRA / ARGB 兼容格式**：如果本机摄像头输出的是与 LiveKit 期望的 BGRA 内存布局兼容的格式，直接调用 `memcpy` 单次拷贝整帧内存。这实现了“零像素转换”极速捕获。
  - **YUYV 格式直转**：针对 Windows 经典摄像头最常见的 `YUYV` 裸数据格式，这里**没有**使用 Qt 的 `toImage()`，而是手写了基于 **BT.601 full-range 的转换公式** 把它直接解算成 `BGRA` 并塞入 LiveKit 专属的内存区。这规避了 Qt 层 `YUYV -> QImage -> ARGB32 -> BGRA` 长达三步的冗余计算路线，大幅缩减了内存开销和本地摄像头的画面延迟。

---

## 2. 视频轨道路由与传输 (LiveKitManager)

采集完毕后的数据封装为 LiveKit 帧序列，它需要注册并发送给服务器推流网络。该部分位于 `src/livekitmanager.cpp` 中。

### 2.1 轨道创建与挂载
- **创建轨道**：在初始阶段，系统申请了 `m_lkVideoSource` (带 `VIDEO_WIDTH` 和 `VIDEO_HEIGHT` 尺寸声明)，随后利用其创建出了 `m_lkVideoTrack`对象 (`livekit::LocalVideoTrack::createLocalVideoTrack`)。
- **发布轨道**：当用户点击界面上的“开启摄像头”并在成功连接房间后，会执行 `doPublishCameraTrack()` 逻辑。内部实质是调用 WebRTC/LiveKit 客户端 SDK 暴露的方法 `room->local_participant()->publish_track(m_lkVideoTrack, options)`，正式让该摄像头接入流媒体推送网络。

### 2.2 带宽与网络控制（底层原理）
LiveKit C++ SDK 基于 Google 官方的 libwebrtc。这意味着视频在网络上传输的过程对于我们而言是**黑盒封装的**：
- **Jitter Buffer (抖动缓冲区)**: 处理网络传输丢包抖动。
- **Bandwidth Estimation (带宽估计, BCC/GCC)**: 实时侦测可用带宽以动态调节 WebRTC 的封包速率。
- **Simulcast (大小流)**: 本程序刻意指定尺寸为静态的 `640x480` 的原因在于，若向底层传入 1080P，SDK 的 Simulcast 机制在稍弱网环境下可能会猛烈地自动将低质量层（如 `320x240`）送给对端导致对端画面异常糊，目前锁死中等分辨率在局域网内拥有最优体验和更少的不确定性。

---

## 3. 远端接收与呈现 (RemoteVideoRenderer)

当其他用户的视频流到达本地时，通过 LiveKit 的回调激活呈现线程，逻辑全部集中于 `src/remotevideorenderer.h/cpp`。

### 3.1 监听与挂载
- 当远端参会者发布自己的摄像头并在我们这里成功被订阅时，会触发 SDK 推入系统的 `LiveKitRoomDelegate::onTrackSubscribed` 回调。
- `LiveKitManager` 会以此为依据创建出一个 `RemoteVideoRenderer` 单例，并将它与某个来自 QML (前端) 的 `QVideoSink` (画面最终渲染终点) 进行绑定（详见 `setExternalVideoSink`）。

### 3.2 独立后台线程解码与渲染 (`RemoteVideoRenderer::renderLoop`)
- 通过 `livekit::VideoStream::fromTrack` 获取远端的数据流（参数设置为容纳 `BGRA` 格式）。
  - **[优化点参数]**：配置缓存大小 `options.capacity = 10`。这相当于给予远端视频在进入本地图形管线前提供了一个额外的、长达大概 400ms 宽度的防抖动序列 `ring-buffer` （这也是一种软件层面的抗网络波动的 Jitter buffer）。
- **`renderLoop` 阻塞读流**：这在一个单独的子线程 `m_renderThread` 中进行。调用 `m_videoStream->read()` 是个阻塞式方法 —— 当没有远端帧到达时它安静地待在后台等待；一旦网络抛过来一张帧立刻唤醒继续执行。
- **像素解包** (详见 `convertFrame`)：将 SDK 抛出的 `livekit::VideoFrame` (BGRA排列) 直接用 `memcpy` **内存复制** 给 Qt 体系内的 `QVideoFrame(Format_BGRA8888)`，无需由于像素大小端不匹配进行重映射，保证在对远端流渲染时能跑满 60Hz 刷新而不卡顿主 UI 线程。
- **安全渲染推送**：主界面 QML 的 `QVideoSink::setVideoFrame()` 内部涉及 GPU 纹理的重新拷贝与显存操作。这里在加锁 `QMutexLocker locker(&m_mutex);` 安全取得最新的共享组件指针后，调用 Qt6 线程安全的 `sinkPointer->setVideoFrame(qtFrame)` 投递入图形处理管线供用户观看。

---

## 4. 屏幕共享管道 (ScreenCapture)

屏幕共享在逻辑上同样属于视频输入的一种，但它的采集机制与摄像头完全不同。涉及的核心代码位于 `src/screencapture.h/cpp` 或集成在 `LiveKitManager` 中。

### 4.1 核心链路
1. **获取屏幕/窗口源**：通过 Qt 的 `QScreen` 或原生 API 获取桌面画面。
2. **高频截图与编码**：通过定时器 (`QTimer`) 或系统的屏幕刷新回调，以大概 15~30fps 的帧率抓取屏幕图层。
3. **转换为 LiveKit 帧**：与摄像头类似，截图后的 `QImage` (通常是 ARGB32) 需要转换为 `BGRA` 格式的 `livekit::VideoFrame`。
4. **推流发布**：调用 SDK 的 `publish_track` 将屏幕共享作为额外的视频轨道（`TrackKind::Video`，但带有 `screen_share` 业务标签）推送给对端。

### 4.2 关键参数与优化
- **分辨率控制 (Scale-down)**：屏幕分辨率通常极高（如 2K/4K），直接推送会瞬间打满带宽。通常会在抓取后，或者在 `livekit::VideoSource` 发送前，判断如果宽或高超出某一阈值（例如宽大于 1920），则按比例进行 `scaled()` 缩小。
- **帧率差异**：屏幕共享通常不需要 30fps，很多时候 10~15fps 足以看清 PPT 和代码。降低采集定时器的频率可以大幅减少 CPU 的截图开销。

---

## 5. QML 前端与 C++ 视频后台的交互机制

项目使用 QML 作为用户界面，因此 C++ 底层的视频数据必须能够“上浮”到 QML 组件中才能被看到。

### 5.1 QVideoSink 的桥梁作用
- `QVideoSink` 是 Qt 提供的用于接收 `QVideoFrame` 的标准接收器。在 QML 中，`<VideoOutput>` 组件自带一个隐藏的 `videoSink` 属性。
- **本地摄像头预览**：
  在 QML 中，我们将 `VideoOutput` 的 `videoSink` 传递给 C++ 的 `mediaCapture.videoSink`。底层摄像头将画面推入内部的 sink 处理完大小和格式后，不仅传给 LiveKit 发送，还会利用本地信号 `videoFrameChanged`，将帧“转发”给前端 QML 传入的外部 sink进行画面预览。
- **远端参会者画面**：
  当 QML 界面（如网格布局）中动态创建一个代表其他参会者的 `VideoOutput` 时，会调用 `liveKitManager.setRemoteVideoSink(participantIdentity, videoSink)`。这会让 `LiveKitManager` 根据 ID 找到对应的 `RemoteVideoRenderer`，并把这个最终渲染接头插上去。

### 5.2 渲染生命周期防崩溃 (QPointer)
- **参数解耦**：当房间内的参会者因为退出会议而动态销毁 QML 界面组件时，对应的 `QVideoSink` 对象会被直接释放（如果底层 C++ 还在往里推画面，将导致悬垂指针引发崩溃）。
- **优化原理 (关键设计)**：在底层的 `RemoteVideoRenderer` 和 `MediaCapture` 中，外部传入的 Sink 被储存在 `QPointer<QVideoSink>` 智能指针类型中。一旦前端 QML 组件被销毁，这个指针会**安全地自动变为 nullptr**。底层的循环在调用 `sinkPointer->setVideoFrame()` 之前只要用 `if (sinkPointer)` 判断其是否非空，就能完美避开段错误 (Segfault)。

---

## 6. 线程模型与防卡顿原理 (并发安全)

音视频处理是高度计算密集型的任务，必须严谨划分线程，如果在主线程里塞入编解码或网络等待，UI 必定卡死。

### 6.1 线程域划分
工程实际上拥有**三类完全隔离的线程域**：
1. **主线程 (QML GUI Thread)**：只负责用户点击事件、界面动画更新、绘制准备。绝对不能执行任何耗时 IO 或死循环操作。
2. **LiveKit / WebRTC 内部工作线程 (SDK Threads)**：这是 LiveKit C++ SDK 自带的黑盒线程池，负责网络的收发包、音视频重采样、RTP 打包和解包等。这部分是纯后台的，由库自行调配。
3. **C++ 拦截 / 渲染工作线程 (Render Threads)**：
   - 比如 `RemoteVideoRenderer` 内部开辟的 `m_renderThread`（即 `std::thread`）。
   - 它的使命是跑 `while(running)` 循环，**阻塞式**地向 SDK 缓存请求取帧（`m_videoStream->read(event)`）。
   - 拿到帧后进行内存 copy 转换，最后投递给主线程 UI。

### 6.2 跨线程数据传递
- 子线程拿到了帧之后，通过 `QVideoSink` 绘制画面利用了 Qt 自身的线程安全性。
- 即便我们在 `m_renderThread` 子线程极高频地直接触发了帧更替（`sinkPointer->setVideoFrame(qtFrame)`），底层 Qt 6 的 RHI (Rendering Hardware Interface) 也会利用事件队列机制，跨线程地通知主线程在下一个**垂直同步周期 (V-Sync)** 到来时，再去显存里抓取这张新图纸。这既确保了多线程的安全，也保证了画面切分犹如原生视频一样丝滑，不会产生画面水平撕裂。
