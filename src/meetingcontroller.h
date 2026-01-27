#ifndef MEETINGCONTROLLER_H
#define MEETINGCONTROLLER_H

#include <QObject>
#include <QString>
#include <QVariantList>

// 前向声明
class LiveKitManager;

class MeetingController : public QObject {
  Q_OBJECT

  // 属性定义
  Q_PROPERTY(bool isMicOn READ isMicOn WRITE setMicOn NOTIFY micOnChanged)
  Q_PROPERTY(
      bool isCameraOn READ isCameraOn WRITE setCameraOn NOTIFY cameraOnChanged)
  Q_PROPERTY(bool isScreenSharing READ isScreenSharing WRITE setScreenSharing
                 NOTIFY screenSharingChanged)
  Q_PROPERTY(bool isRecording READ isRecording WRITE setRecording NOTIFY
                 recordingChanged)
  Q_PROPERTY(bool isHandRaised READ isHandRaised WRITE setHandRaised NOTIFY
                 handRaisedChanged)
  Q_PROPERTY(bool isInMeeting READ isInMeeting WRITE setInMeeting NOTIFY
                 inMeetingChanged)
  Q_PROPERTY(QString meetingId READ meetingId WRITE setMeetingId NOTIFY
                 meetingIdChanged)
  Q_PROPERTY(
      QString userName READ userName WRITE setUserName NOTIFY userNameChanged)
  Q_PROPERTY(QString meetingTitle READ meetingTitle WRITE setMeetingTitle NOTIFY
                 meetingTitleChanged)
  Q_PROPERTY(
      int participantCount READ participantCount NOTIFY participantCountChanged)
  Q_PROPERTY(QString meetingDuration READ meetingDuration NOTIFY
                 meetingDurationChanged)

  // 新增：连接状态属性
  Q_PROPERTY(bool isConnecting READ isConnecting NOTIFY connectingChanged)
  Q_PROPERTY(bool isConnected READ isConnected NOTIFY connectedChanged)

  // 新增：暴露 LiveKitManager 给 QML
  Q_PROPERTY(LiveKitManager *liveKitManager READ liveKitManager CONSTANT)

public:
  explicit MeetingController(QObject *parent = nullptr);
  ~MeetingController();

  // Getter方法
  bool isMicOn() const;
  bool isCameraOn() const;
  bool isScreenSharing() const;
  bool isRecording() const;
  bool isHandRaised() const;
  bool isInMeeting() const;
  QString meetingId() const;
  QString userName() const;
  QString meetingTitle() const;
  int participantCount() const;
  QString meetingDuration() const;

  // 新增：连接状态 Getter
  bool isConnecting() const;
  bool isConnected() const;

  // 获取 LiveKitManager 指针（供外部使用）
  LiveKitManager *liveKitManager() const;

  // Setter方法
  void setMicOn(bool on);
  void setCameraOn(bool on);
  void setScreenSharing(bool sharing);
  void setRecording(bool recording);
  void setHandRaised(bool raised);
  void setInMeeting(bool inMeeting);
  void setMeetingId(const QString &id);
  void setUserName(const QString &name);
  void setMeetingTitle(const QString &title);

public slots:
  // 会议控制方法
  void createMeeting(const QString &title);
  void joinMeeting(const QString &meetingId, const QString &password = "");
  void leaveMeeting();
  void endMeeting();

  // 媒体控制方法
  void toggleMic();
  void toggleCamera();
  void toggleScreenShare();
  void toggleRecording();
  void toggleHandRaise();

  // 其他功能
  void inviteParticipants();
  void openSettings();
  void copyMeetingInfo();
  void switchView(const QString &viewType); // grid, speaker, gallery

  // 登录相关
  void login(const QString &username, const QString &password);
  void registerUser(const QString &username, const QString &password);
  void logout();

  // 保存密码功能
  Q_INVOKABLE void saveCredentials(const QString &username,
                                   const QString &password);
  Q_INVOKABLE void clearSavedCredentials();
  Q_INVOKABLE QString getSavedUsername() const;
  Q_INVOKABLE QString getSavedPassword() const;
  Q_INVOKABLE bool hasRememberedPassword() const;

signals:
  void micOnChanged();
  void cameraOnChanged();
  void screenSharingChanged();
  void recordingChanged();
  void handRaisedChanged();
  void inMeetingChanged();
  void meetingIdChanged();
  void userNameChanged();
  void meetingTitleChanged();
  void participantCountChanged();
  void meetingDurationChanged();

  // 新增：连接状态信号
  void connectingChanged();
  void connectedChanged();

  // 事件信号
  void meetingCreated(const QString &meetingId);
  void meetingJoined();
  void meetingLeft();
  void meetingEnded();
  void loginSuccess();
  void loginFailed(const QString &reason);
  void registerSuccess();
  void registerFailed(const QString &error);
  void errorOccurred(const QString &error);
  void showMessage(const QString &message);

private slots:
  // LiveKit 事件处理槽
  void onLiveKitConnected();
  void onLiveKitDisconnected();
  void onLiveKitError(const QString &error);

private:
  void updateMeetingDuration();
  void startDurationTimer();
  void stopDurationTimer();
  void setupLiveKitConnections(); // 新增：设置 LiveKit 信号连接

private:
  bool m_isMicOn;
  bool m_isCameraOn;
  bool m_isScreenSharing;
  bool m_isRecording;
  bool m_isHandRaised;
  bool m_isInMeeting;
  QString m_meetingId;
  QString m_userName;
  QString m_meetingTitle;
  int m_participantCount;
  int m_meetingSeconds;
  class QTimer *m_durationTimer;

  // 新增：LiveKit 管理器
  LiveKitManager *m_liveKitManager;

  // 用户密码（用于认证）
  QString m_userPassword;
};

#endif // MEETINGCONTROLLER_H
