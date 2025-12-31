#include <QGuiApplication>       // Qt核心GUI类：提供GUI应用程序的基础框架，管理事件循环、窗口系统等
#include <QQmlApplicationEngine> // QML引擎：负责加载、解析和管理QML文件，是QML应用的核心
#include <QQmlContext>           // QML上下文：用于在C++和QML之间共享数据和对象
#include <QQuickStyle>           // 样式设置：用于设置Qt Quick Controls 2的视觉样式（如Material、Fusion等）
#include <QIcon>                 // 图标类：用于设置应用程序图标（当前代码未使用）

// 业务逻辑类：会议控制器、参与者模型、聊天模型
#include "meetingcontroller.h"
#include "participantmodel.h"
#include "chatmodel.h"
#include "livekitmanager.h" // LiveKit 管理器：负责与服务器通信

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
    qmlRegisterType<LiveKitManager>("Meeting", 1, 0, "LiveKitManager"); // 注册 LiveKit 管理器

    // ========== 6. QML引擎和对象实例化 ==========
    // 创建QML引擎，负责加载QML文件、管理QML对象的生命周期
    QQmlApplicationEngine engine;

    // 创建C++业务逻辑对象实例，这些对象将被暴露给QML使用
    // 这种方式创建的是单例对象，整个应用共享同一个实例
    MeetingController meetingController;
    ParticipantModel participantModel;
    ChatModel chatModel;

    // ========== 7. 上下文属性设置 ==========
    // 将C++对象注册为QML全局属性，使其可以在QML中直接访问
    // 原理：通过根上下文将对象暴露给整个QML环境
    // 在QML中可以直接使用：meetingController.someMethod() 或 participantModel.count
    engine.rootContext()->setContextProperty("meetingController", &meetingController);
    engine.rootContext()->setContextProperty("participantModel", &participantModel);
    engine.rootContext()->setContextProperty("chatModel", &chatModel);

    // 将 LiveKitManager 也暴露给 QML（通过 MeetingController 获取）
    engine.rootContext()->setContextProperty("liveKitManager", meetingController.liveKitManager());

    // ========== 8. 加载QML文件 ==========
    // 指定主QML文件的路径
    // qrc:/ 前缀表示从Qt资源系统加载（资源已编译进可执行文件）
    const QUrl url(QStringLiteral("qrc:/qml/main.qml"));

    // ========== 9. 错误处理 ==========
    // 连接QML对象创建失败的信号，如果创建失败则退出应用程序
    // 使用Qt的信号槽机制监听引擎错误，返回码-1表示异常退出
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, []()
                     { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    // ========== 10. 加载和启动 ==========
    // 加载并实例化QML文件，创建用户界面
    engine.load(url);

    // 验证QML根对象是否成功创建，如果失败则退出
    if (engine.rootObjects().isEmpty())
        return -1;

    // 启动Qt事件循环，应用程序将在此运行，直到用户退出
    // 事件循环负责处理所有的用户交互、信号槽、定时器等
    return app.exec();
}
