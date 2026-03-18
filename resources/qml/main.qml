import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15

import "pages"
import "components"
import "dialogs"

ApplicationWindow {
    id: mainWindow
    
    width: 1280
    height: 800
    minimumWidth: 960
    minimumHeight: 640
    visible: true
    title: qsTr("视频会议")
    
    // Material主题设置
    Material.theme: Material.Dark
    Material.accent: "#1E90FF"
    Material.primary: "#1E90FF"
    Material.background: "#1A1A2E"
    
    // 主题颜色
    readonly property color primaryColor: "#1E90FF"
    readonly property color backgroundColor: "#1A1A2E"
    readonly property color surfaceColor: "#252542"
    readonly property color textColor: "#FFFFFF"
    readonly property color textSecondary: "#B0B0C0"
    
    color: backgroundColor
    
    // 监听 meetingController 的摄像头/麦克风状态变化，同步更新本地用户的 ParticipantModel
    Connections {
        target: meetingController
        
        function onCameraOnChanged() {
            // 更新 ParticipantModel 中本地用户 (id="self") 的摄像头状态
            participantModel.updateParticipant("self", meetingController.isMicOn, meetingController.isCameraOn)
            console.log("[main.qml] 摄像头状态变化:", meetingController.isCameraOn)
        }
        
        function onMicOnChanged() {
            // 更新 ParticipantModel 中本地用户的麦克风状态
            participantModel.updateParticipant("self", meetingController.isMicOn, meetingController.isCameraOn)
            console.log("[main.qml] 麦克风状态变化:", meetingController.isMicOn)
        }

        function onScreenSharingChanged() {
            participantModel.setParticipantScreenSharing("self", meetingController.isScreenSharing)
            console.log("[main.qml] 屏幕共享状态变化:", meetingController.isScreenSharing)
        }
    }
    
    // 监听 LiveKitManager 的参会者加入/离开事件
    Connections {
        target: liveKitManager
        
        function onParticipantJoined(identity, name) {
            console.log("[main.qml] 远程参会者加入:", identity, name)
            participantModel.addParticipant(identity, name, false, false)
        }
        
        function onParticipantLeft(identity) {
            console.log("[main.qml] 远程参会者离开:", identity)
            participantModel.removeParticipant(identity)
        }
        
        // 【关键修复】当远程视频轨道订阅时，更新参会者的摄像头状态
        function onTrackSubscribed(participantIdentity, trackSid, trackKind, trackSource) {
            console.log("[main.qml] 轨道订阅:", participantIdentity, trackSid, "kind:", trackKind, "source:", trackSource)
            // trackKind: 1 = Audio, 2 = Video
            // trackSource: 1 = Camera, 3 = ScreenShare
            if (trackKind === 2) {
                if (trackSource === 1) {
                    console.log("[main.qml] 更新参会者摄像头状态为开启:", participantIdentity)
                    participantModel.updateParticipantCamera(participantIdentity, true)
                } else if (trackSource === 3) {
                    console.log("[main.qml] 更新参会者屏幕共享状态为开启:", participantIdentity)
                    participantModel.setParticipantScreenSharing(participantIdentity, true)
                }
            }
        }
        
        // 【关键修复】当远程视频轨道取消订阅时，更新参会者的摄像头状态
        function onTrackUnsubscribed(participantIdentity, trackSid, trackKind, trackSource) {
            console.log("[main.qml] 轨道取消订阅:", participantIdentity, trackSid, "kind:", trackKind, "source:", trackSource)
            if (trackKind === 2) {
                if (trackSource === 1) {
                    participantModel.updateParticipantCamera(participantIdentity, false)
                } else if (trackSource === 3) {
                    participantModel.setParticipantScreenSharing(participantIdentity, false)
                }
            }
        }

        function onTrackMuted(participantIdentity, trackSid, trackKind, trackSource, muted) {
            console.log("[main.qml] 轨道静音状态变化:", participantIdentity, trackSid, "kind:", trackKind, "source:", trackSource, "muted:", muted)
            if (trackKind !== 2) {
                return
            }

            if (trackSource === 1) {
                participantModel.updateParticipantCamera(participantIdentity, !muted)
            } else if (trackSource === 3) {
                participantModel.setParticipantScreenSharing(participantIdentity, !muted)
            }
        }
        
        function onConnected() {
            console.log("[main.qml] LiveKit 连接成功，添加本地用户并进入会议室")
            // 添加本地用户到参会者列表
            participantModel.addParticipant("self", meetingController.userName, true, true)
            
            // 【关键】连接成功后才跳转到会议室页面
            // 检查当前页面，避免重复跳转
            if (stackView.currentItem !== meetingRoomPage) {
                stackView.push(meetingRoomPage)
            }
        }
        
        function onDisconnected() {
            console.log("[main.qml] LiveKit 断开连接，清空参会者列表")
            // 【关键修复】添加空值检查，避免程序关闭时崩溃
            if (participantModel) {
                participantModel.clear()
            }
        }
        
    }
    
    // 页面堆栈
    StackView {
        id: stackView
        anchors.fill: parent
        // 正式发布：从登录页开始
        initialItem: loginPage
        
        // 过渡动画
        pushEnter: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: 200
            }
        }
        pushExit: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: 200
            }
        }
        popEnter: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: 200
            }
        }
        popExit: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: 200
            }
        }
    }
    
    // 页面组件
    Component {
        id: loginPage
        LoginPage {
            onLoginSuccess: {
                stackView.replace(homePage)
            }
            onGoToRegister: {
                stackView.push(registerPage)
            }
        }
    }
    
    Component {
        id: registerPage
        RegisterPage {
            onRegisterSuccess: {
                stackView.replace(homePage)
            }
            onGoToLogin: {
                stackView.pop()
            }
        }
    }
    
    Component {
        id: homePage
        HomePage {
            onJoinMeeting: function(meetingId) {
                // 显示加载对话框
                connectionLoadingDialog.open()
                // 调用 joinMeeting，页面跳转在 onConnected 中处理
                meetingController.joinMeeting(meetingId)
            }
            onCreateMeeting: function(title) {
                // 显示加载对话框
                connectionLoadingDialog.open()
                // 调用 createMeeting，页面跳转在 onConnected 中处理
                meetingController.createMeeting(title)
            }
            onLogout: {
                meetingController.logout()
                stackView.replace(loginPage)
            }
        }
    }
    
    Component {
        id: meetingRoomPage
        MeetingRoom {
            onLeaveMeeting: {
                meetingController.leaveMeeting()
                participantModel.clear()
                chatModel.clear()
                stackView.pop()
            }
        }
    }
    
    // 全局对话框
    JoinMeetingDialog {
        id: joinMeetingDialog
        onAccepted: {
            if (meetingId.length > 0) {
                // 显示加载对话框
                connectionLoadingDialog.open()
                // 调用 joinMeeting，页面跳转在 onConnected 中处理
                meetingController.joinMeeting(meetingId)
            }
        }
    }
    
    CreateMeetingDialog {
        id: createMeetingDialog
        onAccepted: {
            // 显示加载对话框
            connectionLoadingDialog.open()
            // 调用 createMeeting，页面跳转在 onConnected 中处理
            meetingController.createMeeting(meetingTitle)
        }
    }
    
    SettingsDialog {
        id: settingsDialog
    }

    ScheduleMeetingDialog {
        id: scheduleMeetingDialog
    }

    VideoPreviewDialog {
        id: videoPreviewDialog
    }
    
    // 连接等待对话框
    ConnectionLoadingDialog {
        id: connectionLoadingDialog
    }
    
    // 消息提示
    Rectangle {
        id: messageToast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 100
        width: toastText.width + 40
        height: 40
        radius: 20
        color: "#333355"
        opacity: 0
        visible: opacity > 0
        
        Text {
            id: toastText
            anchors.centerIn: parent
            color: textColor
            font.pixelSize: 14
        }
        
        Behavior on opacity {
            NumberAnimation { duration: 200 }
        }
        
        Timer {
            id: toastTimer
            interval: 2000
            onTriggered: messageToast.opacity = 0
        }
    }
    
    // 显示消息函数
    function showMessage(message) {
        toastText.text = message
        messageToast.opacity = 1
        toastTimer.restart()
    }
    
    // 连接控制器信号
    Connections {
        target: meetingController
        function onShowMessage(message) {
            showMessage(message)
        }
    }
}
