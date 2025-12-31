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
    }
    
    // 页面堆栈
    StackView {
        id: stackView
        anchors.fill: parent
        // 开发阶段：直接进入 homePage 跳过登录
        // 正式发布时改回 loginPage
        initialItem: homePage
        
        // 开发阶段：设置默认用户名（跳过登录时使用）
        Component.onCompleted: {
            meetingController.userName = "开发测试用户"
        }
        
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
                meetingController.joinMeeting(meetingId)
                participantModel.addDemoParticipants()
                stackView.push(meetingRoomPage)
            }
            onCreateMeeting: function(title) {
                meetingController.createMeeting(title)
                participantModel.addDemoParticipants()
                stackView.push(meetingRoomPage)
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
                meetingController.joinMeeting(meetingId)
                participantModel.addDemoParticipants()
                stackView.push(meetingRoomPage)
            }
        }
    }
    
    CreateMeetingDialog {
        id: createMeetingDialog
        onAccepted: {
            meetingController.createMeeting(meetingTitle)
            participantModel.addDemoParticipants()
            stackView.push(meetingRoomPage)
        }
    }
    
    SettingsDialog {
        id: settingsDialog
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
