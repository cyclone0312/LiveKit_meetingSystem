#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>

#include "meetingcontroller.h"
#include "participantmodel.h"
#include "chatmodel.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // 设置应用信息
    app.setApplicationName("Meeting");
    app.setOrganizationName("MeetingApp");
    app.setApplicationVersion("1.0.0");

    // 设置Quick Controls样式
    QQuickStyle::setStyle("Material");

    // 注册QML类型
    qmlRegisterType<MeetingController>("Meeting", 1, 0, "MeetingController");
    qmlRegisterType<ParticipantModel>("Meeting", 1, 0, "ParticipantModel");
    qmlRegisterType<ChatModel>("Meeting", 1, 0, "ChatModel");

    QQmlApplicationEngine engine;

    // 创建控制器实例
    MeetingController meetingController;
    ParticipantModel participantModel;
    ChatModel chatModel;

    // 设置上下文属性
    engine.rootContext()->setContextProperty("meetingController", &meetingController);
    engine.rootContext()->setContextProperty("participantModel", &participantModel);
    engine.rootContext()->setContextProperty("chatModel", &chatModel);

    // 加载主QML文件
    const QUrl url(QStringLiteral("qrc:/qml/main.qml"));

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, []()
                     { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    engine.load(url);

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
