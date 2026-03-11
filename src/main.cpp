#include <QGuiApplication>       // Qt核心GUI类：提供GUI应用程序的基础框架，管理事件循环、窗口系统等
#include <QIcon>                 // 图标类：用于设置应用程序图标（当前代码未使用）
#include <QQmlApplicationEngine> // QML引擎：负责加载、解析和管理QML文件，是QML应用的核心
#include <QQmlContext>           // QML上下文：用于在C++和QML之间共享数据和对象
#include <QQuickStyle>           // 样式设置：用于设置Qt Quick Controls 2的视觉样式（如Material、Fusion等）
#include <QTimer>

// 业务逻辑类：会议控制器、参与者模型、聊天模型
#include "aiassistant.h"
#include "audiomixer.h" // 音频混音器：混合本地+远程音频
#include "chatmodel.h"
#include "livekitmanager.h" // LiveKit 管理器：负责与服务器通信
#include "mediacapture.h"   // 媒体采集器：负责摄像头和麦克风采集
#include "meetingcontroller.h"
#include "meetingrecorder.h" // 视频录制器
#include "participantmodel.h"
#include "remoteaudioplayer.h"
#include "remotevideorenderer.h"
#include "screencapture.h"
#include "videocompositor.h" // 视频合成器

// LiveKit SDK shutdown
#include <livekit/livekit.h>

int main(int argc, char *argv[])
{
  // ========== 2. 应用程序初始化 ==========
  // 创建GUI应用程序对象，初始化Qt事件循环
  // 这是Qt应用的核心，管理应用程序的控制流、事件处理（鼠标、键盘、窗口事件等）
  QGuiApplication app(argc, argv);

  // ========== 3. 应用程序元数据设置 ==========
  // 设置应用程序的标识信息，用于：
  // - 配置文件存储路径（如 ~/.config/MeetingApp/Meeting.conf）
  // - Windows注册表键名
  // - 关于对话框中的信息显示
  app.setApplicationName("Meeting");
  app.setOrganizationName("MeetingApp");
  app.setApplicationVersion("1.0.0");

  // ========== 4. 样式设置 ==========
  // 设置Qt Quick Controls 2的视觉样式为Material Design
  // 注意：必须在加载任何QML文件之前调用，否则无效
  // 可选样式：Material、Universal、Fusion、Imagine等
  QQuickStyle::setStyle("Material");

  // ========== 5. QML类型注册 ==========
  // 将C++类注册到QML类型系统，使其可以在QML中作为类型使用
  // 参数说明：qmlRegisterType<C++类>("模块名", 主版本, 次版本, "QML类型名")
  // 在QML中使用：import Meeting 1.0 后可以创建 MeetingController { }
  qmlRegisterType<MeetingController>("Meeting", 1, 0, "MeetingController");
  qmlRegisterType<ParticipantModel>("Meeting", 1, 0, "ParticipantModel");
  qmlRegisterType<ChatModel>("Meeting", 1, 0, "ChatModel");
  qmlRegisterType<LiveKitManager>("Meeting", 1, 0,
                                  "LiveKitManager"); // 注册 LiveKit 管理器
  qmlRegisterType<MediaCapture>("Meeting", 1, 0,
                                "MediaCapture"); // 注册媒体采集器

  // 注册不可创建类型（用于属性类型识别）
  qmlRegisterUncreatableType<VideoFrameHandler>(
      "Meeting", 1, 0, "VideoFrameHandler", "Cannot create VideoFrameHandler");
  qmlRegisterUncreatableType<AudioFrameHandler>(
      "Meeting", 1, 0, "AudioFrameHandler", "Cannot create AudioFrameHandler");

  // ========== 6. QML引擎和对象实例化 ==========
  // 创建QML引擎，负责加载QML文件、管理QML对象的生命周期
  QQmlApplicationEngine engine;

  // 创建C++业务逻辑对象实例，这些对象将被暴露给QML使用
  // 这种方式创建的是单例对象，整个应用共享同一个实例
  MeetingController meetingController;
  ParticipantModel participantModel;
  ChatModel chatModel;
  AIAssistant aiAssistant;
  AudioMixer audioMixer;

  // ========== 7. 上下文属性设置 ==========
  // 将C++对象注册为QML全局属性，使其可以在QML中直接访问
  // 原理：通过根上下文将对象暴露给整个QML环境
  // 在QML中可以直接使用：meetingController.someMethod() 或
  // participantModel.count
  engine.rootContext()->setContextProperty("meetingController",
                                           &meetingController);
  engine.rootContext()->setContextProperty("participantModel",
                                           &participantModel);
  engine.rootContext()->setContextProperty("chatModel", &chatModel);

  // 将 LiveKitManager 也暴露给 QML（通过 MeetingController 获取）
  engine.rootContext()->setContextProperty("liveKitManager",
                                           meetingController.liveKitManager());

  // 将 MediaCapture 暴露给 QML（用于视频预览等）
  engine.rootContext()->setContextProperty(
      "mediaCapture", meetingController.liveKitManager()->mediaCapture());

  // 将 AIAssistant 暴露给 QML
  engine.rootContext()->setContextProperty("aiAssistant", &aiAssistant);

  // ========== 信号连接：聊天消息收发 ==========
  // 收到远程聊天消息 → 添加到 ChatModel 显示
  QObject::connect(meetingController.liveKitManager(),
                   &LiveKitManager::chatMessageReceived, &chatModel,
                   [&chatModel](const QString &senderId,
                                const QString &senderName,
                                const QString &message)
                   {
                     chatModel.addMessage(senderId, senderName, message, false);
                   });

  // ========== 信号连接：AI 助手 ==========
  // 加入房间时同步房间名、用户名和服务器地址到 AI 助手
  QObject::connect(
      meetingController.liveKitManager(), &LiveKitManager::connected,
      &aiAssistant, [&aiAssistant, &meetingController]()
      {
        aiAssistant.setServerUrl(
            meetingController.liveKitManager()->tokenServerUrl());
        aiAssistant.setRoomName(
            meetingController.liveKitManager()->currentRoom());
        aiAssistant.setUserName(
            meetingController.liveKitManager()->currentUser());
        qDebug() << "[main] AI 助手已同步: serverUrl="
                 << meetingController.liveKitManager()->tokenServerUrl()
                 << "room=" << meetingController.liveKitManager()->currentRoom()
                 << "user="
                 << meetingController.liveKitManager()->currentUser(); });

  // 离开房间时自动保存录音并清空房间名
  QObject::connect(meetingController.liveKitManager(),
                   &LiveKitManager::disconnected, &aiAssistant,
                   [&aiAssistant]()
                   {
                     // 如果正在录音，自动保存到本地文件
                     if (aiAssistant.isRecordingAudio())
                     {
                       qDebug() << "[main] 会议断开，自动保存录音";
                       aiAssistant.saveRecordingToFile();
                     }
                     aiAssistant.setRoomName("");
                   });

  // ========== 录制按钮联动 AI 录音 ==========
  // 点击视频录制按钮时，自动启动/停止 AI 音频录音
  QObject::connect(&meetingController, &MeetingController::recordingChanged,
                   &aiAssistant, [&meetingController, &aiAssistant]()
                   {
                     if (meetingController.isRecording()) {
                       // 开始录制 → 自动启动 AI 录音
                       qDebug() << "[main] 录制按钮开启，联动启动 AI 录音";
                       aiAssistant.startRecording();
                     } else {
                       // 停止录制 → 自动保存录音到本地（不立即转录）
                       if (aiAssistant.isRecordingAudio()) {
                         qDebug() << "[main] 录制按钮关闭，联动保存录音";
                         aiAssistant.saveRecordingToFile();
                       }
                     } });

  // 麦克风 PCM 数据 → AudioMixer → AI 语音转录缓冲
  // Phase 1: 混音器收集本地+远程音频，混合后送给 AIAssistant
  MediaCapture *mc = meetingController.liveKitManager()->mediaCapture();
  if (mc)
  {
    // 本地麦克风 → AudioMixer
    QObject::connect(mc, &MediaCapture::rawAudioCaptured, &audioMixer,
                     &AudioMixer::feedLocalAudio);
  }
  // AudioMixer 混合输出 → AIAssistant
  QObject::connect(&audioMixer, &AudioMixer::mixedAudioReady, &aiAssistant,
                   &AIAssistant::feedAudioData);

  // 远程参会者音频 → AudioMixer（在 track 订阅/取消时动态连接）
  LiveKitManager *lkm = meetingController.liveKitManager();
  QObject::connect(lkm, &LiveKitManager::trackSubscribed, &audioMixer,
                   [lkm, &audioMixer](const QString &participantIdentity,
                                      const QString &trackSid, int trackKind)
                   {
                     Q_UNUSED(trackSid)
                     // KIND_AUDIO = 1
                     if (trackKind != 1)
                       return;
                     // 延迟连接：RemoteAudioPlayer 在 QueuedConnection 中创建
                     QTimer::singleShot(200, &audioMixer, [lkm, participantIdentity, &audioMixer]()
                                        {
                       if (lkm->remoteAudioPlayers().contains(participantIdentity)) {
                         auto player = lkm->remoteAudioPlayers()[participantIdentity];
                         if (player) {
                           QObject::connect(
                               player.get(), &RemoteAudioPlayer::audioDataReady,
                               &audioMixer,
                               [&audioMixer, participantIdentity](
                                   QByteArray data, int sampleRate, int channels) {
                                 audioMixer.feedRemoteAudio(participantIdentity,
                                                           data, sampleRate, channels);
                               });
                           qDebug() << "[main] 远程音频已连接到 AudioMixer:"
                                    << participantIdentity;
                         }
                       } });
                   });
  QObject::connect(lkm, &LiveKitManager::participantLeft, &audioMixer,
                   &AudioMixer::removeParticipant);

  // ========== 视频录制：信号连线 ==========
  VideoCompositor *vc = meetingController.videoCompositor();
  MeetingRecorder *mr = meetingController.meetingRecorder();

  // 本地摄像头帧 → VideoCompositor
  if (mc)
  {
    QObject::connect(mc, &MediaCapture::localVideoFrameReady, vc,
                     [vc, &meetingController](const QImage &frame)
                     {
                       vc->feedFrame("local", frame,
                                     meetingController.userName());
                     });
  }

  // 屏幕共享帧 → VideoCompositor
  ScreenCapture *sc = lkm->screenCapture();
  if (sc)
  {
    QObject::connect(sc, &ScreenCapture::screenFrameReady, vc,
                     [vc](const QImage &frame)
                     {
                       vc->feedFrame("screen", frame, "屏幕共享");
                     });
  }

  // 远程视频帧 → VideoCompositor（在 track 订阅时动态连接）
  QObject::connect(
      lkm, &LiveKitManager::trackSubscribed, vc,
      [lkm, vc](const QString &participantIdentity, const QString &trackSid,
                int trackKind)
      {
        Q_UNUSED(trackSid)
        // KIND_VIDEO = 0
        if (trackKind != 0)
          return;
        QTimer::singleShot(200, vc, [lkm, participantIdentity, vc]()
                           {
          if (lkm->remoteVideoRenderers().contains(participantIdentity)) {
            auto renderer =
                lkm->remoteVideoRenderers()[participantIdentity];
            if (renderer) {
              QObject::connect(
                  renderer.get(), &RemoteVideoRenderer::videoFrameReady,
                  vc,
                  [vc](const QString &pid, const QImage &frame) {
                    vc->feedFrame(pid, frame, pid);
                  });
              qDebug() << "[main] 远程视频已连接到 VideoCompositor:"
                       << participantIdentity;
            }
          } });
      });

  // 参会者离开 → 从 VideoCompositor 移除
  QObject::connect(lkm, &LiveKitManager::participantLeft, vc,
                   &VideoCompositor::removeParticipant);

  // AudioMixer 混合音频 → MeetingRecorder（录制音轨）
  QObject::connect(&audioMixer, &AudioMixer::mixedAudioReady, mr,
                   &MeetingRecorder::feedAudioData);

  // 离开会议时自动停止视频录制
  QObject::connect(lkm, &LiveKitManager::disconnected, &meetingController,
                   [&meetingController]()
                   {
                     if (meetingController.isVideoRecording())
                     {
                       qDebug() << "[main] 会议断开，自动停止视频录制";
                       meetingController.stopVideoRecording();
                     }
                   });

  // 视频录制完成 → 保存元数据到本地录制列表
  QObject::connect(mr, &MeetingRecorder::recordingStopped, &aiAssistant,
                   [&aiAssistant, &meetingController](const QString &filePath)
                   {
                     aiAssistant.saveVideoRecordingMeta(
                         filePath,
                         meetingController.meetingId(),
                         meetingController.meetingRecorder()->durationSeconds());
                   });

  // ========== 8. 加载QML文件 ==========
  // 指定主QML文件的路径
  // qrc:/ 前缀表示从Qt资源系统加载（资源已编译进可执行文件）
  const QUrl url(QStringLiteral("qrc:/qml/main.qml"));

  // ========== 9. 错误处理 ==========
  // 连接QML对象创建失败的信号，如果创建失败则退出应用程序
  // 使用Qt的信号槽机制监听引擎错误，返回码-1表示异常退出
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
      []()
      { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

  // ========== 10. 加载和启动 ==========
  // 加载并实例化QML文件，创建用户界面
  engine.load(url);

  // 验证QML根对象是否成功创建，如果失败则退出
  if (engine.rootObjects().isEmpty())
    return -1;

  // 启动Qt事件循环，应用程序将在此运行，直到用户退出
  // 事件循环负责处理所有的用户交互、信号槽、定时器等
  int result = app.exec();

  // 【关键修复】在程序退出前关闭 LiveKit SDK
  // 必须在所有使用 LiveKit 的对象销毁之后调用
  livekit::shutdown();

  return result;
}
