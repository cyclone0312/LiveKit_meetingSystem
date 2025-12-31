#include "meetingcontroller.h"
#include "livekitmanager.h"
#include <QTimer>
#include <QRandomGenerator>
#include <QClipboard>
#include <QGuiApplication>
#include <QDebug>

MeetingController::MeetingController(QObject *parent)
    : QObject(parent), m_isMicOn(false), m_isCameraOn(false), m_isScreenSharing(false), m_isRecording(false), m_isHandRaised(false), m_isInMeeting(false), m_participantCount(0), m_meetingSeconds(0), m_durationTimer(new QTimer(this)), m_liveKitManager(new LiveKitManager(this)) // 创建 LiveKit 管理器
{
    connect(m_durationTimer, &QTimer::timeout, this, &MeetingController::updateMeetingDuration);

    // 设置 LiveKit 信号连接
    setupLiveKitConnections();

    qDebug() << "[MeetingController] 初始化完成，LiveKit 管理器已创建";
}

MeetingController::~MeetingController()
{
    if (m_durationTimer->isActive())
    {
        m_durationTimer->stop();
    }
}

// Getter实现
bool MeetingController::isMicOn() const { return m_isMicOn; }
bool MeetingController::isCameraOn() const { return m_isCameraOn; }
bool MeetingController::isScreenSharing() const { return m_isScreenSharing; }
bool MeetingController::isRecording() const { return m_isRecording; }
bool MeetingController::isHandRaised() const { return m_isHandRaised; }
bool MeetingController::isInMeeting() const { return m_isInMeeting; }
QString MeetingController::meetingId() const { return m_meetingId; }
QString MeetingController::userName() const { return m_userName; }
QString MeetingController::meetingTitle() const { return m_meetingTitle; }
int MeetingController::participantCount() const { return m_participantCount; }

// 新增：连接状态 Getter
bool MeetingController::isConnecting() const { return m_liveKitManager->isConnecting(); }
bool MeetingController::isConnected() const { return m_liveKitManager->isConnected(); }

// 获取 LiveKitManager 指针
LiveKitManager *MeetingController::liveKitManager() const { return m_liveKitManager; }

QString MeetingController::meetingDuration() const
{
    int hours = m_meetingSeconds / 3600;
    int minutes = (m_meetingSeconds % 3600) / 60;
    int seconds = m_meetingSeconds % 60;

    if (hours > 0)
    {
        return QString("%1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    }
    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

// Setter实现
void MeetingController::setMicOn(bool on)
{
    if (m_isMicOn != on)
    {
        m_isMicOn = on;
        emit micOnChanged();
        emit showMessage(on ? "麦克风已开启" : "麦克风已关闭");

        // 同步到 LiveKit
        if (m_liveKitManager)
        {
            if (on)
            {
                // 开启麦克风并发布
                if (m_liveKitManager->isConnected())
                {
                    m_liveKitManager->publishMicrophone();
                }
            }
            else
            {
                // 取消发布并停止麦克风
                if (m_liveKitManager->isConnected())
                {
                    m_liveKitManager->unpublishMicrophone();
                }
                // 无论是否连接，都停止本地麦克风
                if (m_liveKitManager->mediaCapture())
                {
                    m_liveKitManager->mediaCapture()->stopMicrophone();
                }
            }
        }
    }
}

void MeetingController::setCameraOn(bool on)
{
    if (m_isCameraOn != on)
    {
        m_isCameraOn = on;
        emit cameraOnChanged();
        emit showMessage(on ? "摄像头已开启" : "摄像头已关闭");

        // 同步到 LiveKit
        if (m_liveKitManager)
        {
            if (on)
            {
                // 开启摄像头并发布
                if (m_liveKitManager->isConnected())
                {
                    m_liveKitManager->publishCamera();
                }
            }
            else
            {
                // 取消发布并停止摄像头
                if (m_liveKitManager->isConnected())
                {
                    m_liveKitManager->unpublishCamera();
                }
                // 无论是否连接，都停止本地摄像头
                if (m_liveKitManager->mediaCapture())
                {
                    m_liveKitManager->mediaCapture()->stopCamera();
                }
            }
        }
    }
}

void MeetingController::setScreenSharing(bool sharing)
{
    if (m_isScreenSharing != sharing)
    {
        m_isScreenSharing = sharing;
        emit screenSharingChanged();
        emit showMessage(sharing ? "屏幕共享已开始" : "屏幕共享已结束");
    }
}

void MeetingController::setRecording(bool recording)
{
    if (m_isRecording != recording)
    {
        m_isRecording = recording;
        emit recordingChanged();
        emit showMessage(recording ? "录制已开始" : "录制已停止");
    }
}

void MeetingController::setHandRaised(bool raised)
{
    if (m_isHandRaised != raised)
    {
        m_isHandRaised = raised;
        emit handRaisedChanged();
        emit showMessage(raised ? "已举手" : "已放下手");
    }
}

void MeetingController::setInMeeting(bool inMeeting)
{
    if (m_isInMeeting != inMeeting)
    {
        m_isInMeeting = inMeeting;
        emit inMeetingChanged();

        if (inMeeting)
        {
            startDurationTimer();
        }
        else
        {
            stopDurationTimer();
        }
    }
}

void MeetingController::setMeetingId(const QString &id)
{
    if (m_meetingId != id)
    {
        m_meetingId = id;
        emit meetingIdChanged();
    }
}

void MeetingController::setUserName(const QString &name)
{
    if (m_userName != name)
    {
        m_userName = name;
        emit userNameChanged();
    }
}

void MeetingController::setMeetingTitle(const QString &title)
{
    if (m_meetingTitle != title)
    {
        m_meetingTitle = title;
        emit meetingTitleChanged();
    }
}

// 会议控制实现
void MeetingController::createMeeting(const QString &title)
{
    qDebug() << "[MeetingController] 创建会议:" << title;

    // 生成随机会议ID（作为房间名）
    QString newMeetingId = QString::number(QRandomGenerator::global()->bounded(100000000, 999999999));

    setMeetingId(newMeetingId);
    setMeetingTitle(title.isEmpty() ? m_userName + "的会议" : title);

    // 通过 LiveKit 加入房间
    m_liveKitManager->joinRoom(newMeetingId, m_userName);

    emit showMessage("正在创建会议...");
}

void MeetingController::joinMeeting(const QString &meetingId, const QString &password)
{
    Q_UNUSED(password)

    qDebug() << "[MeetingController] 加入会议:" << meetingId;

    if (meetingId.isEmpty())
    {
        emit errorOccurred("请输入会议号");
        return;
    }

    setMeetingId(meetingId);
    setMeetingTitle("会议 " + meetingId);

    // 通过 LiveKit 加入房间
    m_liveKitManager->joinRoom(meetingId, m_userName);

    emit showMessage("正在加入会议...");
}

void MeetingController::leaveMeeting()
{
    qDebug() << "[MeetingController] 离开会议";

    // 先离开 LiveKit 房间
    m_liveKitManager->leaveRoom();

    setInMeeting(false);
    setMicOn(false);
    setCameraOn(false);
    setScreenSharing(false);
    setRecording(false);
    setHandRaised(false);
    setMeetingId("");
    setMeetingTitle("");

    m_participantCount = 0;
    emit participantCountChanged();

    emit meetingLeft();
    emit showMessage("已离开会议");
}

void MeetingController::endMeeting()
{
    emit showMessage("会议已结束");
    leaveMeeting();
    emit meetingEnded();
}

// 媒体控制实现
void MeetingController::toggleMic()
{
    setMicOn(!m_isMicOn);
}

void MeetingController::toggleCamera()
{
    setCameraOn(!m_isCameraOn);
}

void MeetingController::toggleScreenShare()
{
    setScreenSharing(!m_isScreenSharing);
}

void MeetingController::toggleRecording()
{
    setRecording(!m_isRecording);
}

void MeetingController::toggleHandRaise()
{
    setHandRaised(!m_isHandRaised);
}

// 其他功能实现
void MeetingController::inviteParticipants()
{
    QString inviteText = QString("加入会议\n会议主题：%1\n会议号：%2")
                             .arg(m_meetingTitle)
                             .arg(m_meetingId);

    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setText(inviteText);

    emit showMessage("邀请信息已复制到剪贴板");
}

void MeetingController::openSettings()
{
    emit showMessage("打开设置");
}

void MeetingController::copyMeetingInfo()
{
    QString info = QString("会议号：%1").arg(m_meetingId);

    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setText(info);

    emit showMessage("会议信息已复制");
}

void MeetingController::switchView(const QString &viewType)
{
    emit showMessage("切换到" + viewType + "视图");
}

bool MeetingController::login(const QString &username, const QString &password)
{
    // 简单的登录模拟
    if (username.isEmpty() || password.isEmpty())
    {
        emit loginFailed("用户名或密码不能为空");
        return false;
    }

    setUserName(username);
    emit loginSuccess();
    emit showMessage("登录成功，欢迎 " + username);
    return true;
}

void MeetingController::logout()
{
    if (m_isInMeeting)
    {
        leaveMeeting();
    }
    setUserName("");
    emit showMessage("已退出登录");
}

void MeetingController::updateMeetingDuration()
{
    m_meetingSeconds++;
    emit meetingDurationChanged();
}

void MeetingController::startDurationTimer()
{
    m_meetingSeconds = 0;
    m_durationTimer->start(1000);
}

void MeetingController::stopDurationTimer()
{
    m_durationTimer->stop();
    m_meetingSeconds = 0;
}

// ==================== LiveKit 信号连接设置 ====================

void MeetingController::setupLiveKitConnections()
{
    // 连接状态变化
    connect(m_liveKitManager, &LiveKitManager::connectionStateChanged, this, [this]()
            {
        emit connectingChanged();
        emit connectedChanged(); });

    // 连接成功
    connect(m_liveKitManager, &LiveKitManager::connected,
            this, &MeetingController::onLiveKitConnected);

    // 断开连接
    connect(m_liveKitManager, &LiveKitManager::disconnected,
            this, &MeetingController::onLiveKitDisconnected);

    // 错误处理
    connect(m_liveKitManager, &LiveKitManager::errorOccurred,
            this, &MeetingController::onLiveKitError);

    qDebug() << "[MeetingController] LiveKit 信号连接已设置";
}

// ==================== LiveKit 事件处理槽 ====================

void MeetingController::onLiveKitConnected()
{
    qDebug() << "[MeetingController] LiveKit 连接成功，进入会议";

    setInMeeting(true);

    // 初始化参会者数量
    m_participantCount = 1;
    emit participantCountChanged();

    // 发送相应的信号
    if (m_meetingTitle.contains("的会议"))
    {
        // 这是创建的会议
        emit meetingCreated(m_meetingId);
        emit showMessage("会议已创建，会议号：" + m_meetingId);
    }
    else
    {
        // 这是加入的会议
        emit meetingJoined();
        emit showMessage("已成功加入会议");
    }
}

void MeetingController::onLiveKitDisconnected()
{
    qDebug() << "[MeetingController] LiveKit 连接断开";

    if (m_isInMeeting)
    {
        setInMeeting(false);
        emit showMessage("与服务器的连接已断开");
    }
}

void MeetingController::onLiveKitError(const QString &error)
{
    qWarning() << "[MeetingController] LiveKit 错误:" << error;
    emit errorOccurred(error);
    emit showMessage("连接错误: " + error);
}
