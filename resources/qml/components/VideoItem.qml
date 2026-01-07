import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtMultimedia

/**
 * VideoItem - 单个参会者的视频显示组件
 * 
 * 【本地视频显示原理】
 * 摄像头 → QCamera → QMediaCaptureSession → QVideoSink(内部) → onVideoFrameReceived
 *                                                                    ↓
 *                                               bindVideoSink() → QVideoSink(外部) → VideoOutput(QML显示)
 * 
 * 【关键点】
 * 1. mediaCapture 必须通过父组件传递，不能直接访问全局变量（GridView delegate 作用域隔离）
 * 2. 调用 C++ 方法必须使用 Q_INVOKABLE 标记的方法（如 bindVideoSink），
 *    普通方法（如 setVideoSink）无法从 QML 调用
 * 3. VideoOutput.videoSink 是 VideoOutput 用于接收视频帧的对象，需要传给 MediaCapture
 */
Rectangle {
    id: videoItem
    
    property string participantId: ""  // 参会者 ID
    property string participantName: ""
    property bool isMicOn: false
    property bool isCameraOn: false
    property bool isHost: false
    property bool isHandRaised: false
    property bool isScreenSharing: false
    property bool isLocalUser: false  // 是否是本地用户
    
    // 【重要】MediaCapture 对象引用，必须从父组件传入
    // 在 GridView 的 delegate 中无法直接访问全局变量，会得到 null
    // 需要通过 VideoGrid 的属性中转传递
    property var mediaCapture: null
    
    radius: 8
    color: "#252542"
    border.color: isScreenSharing ? "#1E90FF" : "transparent"
    border.width: isScreenSharing ? 2 : 0
    
    // 监听本地 isCameraOn 属性变化（用户点击开启摄像头）
    onIsCameraOnChanged: {
        console.log("[VideoItem] onIsCameraOnChanged: isLocalUser=", isLocalUser, "isCameraOn=", isCameraOn, "mediaCapture=", mediaCapture ? "有效" : "null")
        if (isLocalUser && isCameraOn && mediaCapture) {
            console.log("[VideoItem] isCameraOn 变为 true，准备绑定 VideoSink")
            // 延迟绑定，确保 VideoOutput 已经准备好
            bindVideoSinkTimer.start()
        }
    }
    
    // 监听全局摄像头状态变化（针对本地用户）
    Connections {
        target: isLocalUser ? meetingController : null
        
        function onCameraOnChanged() {
            console.log("[VideoItem] meetingController.cameraOnChanged: isCameraOn=", meetingController.isCameraOn)
            if (isLocalUser && meetingController.isCameraOn && mediaCapture) {
                console.log("[VideoItem] 通过 Connections 检测到摄像头开启，准备绑定 VideoSink")
                bindVideoSinkTimer.start()
            }
        }
    }
    
    // 定时器用于延迟绑定
    // 【为什么需要延迟】VideoOutput 创建后，其 videoSink 属性可能还未初始化完成
    Timer {
        id: bindVideoSinkTimer
        interval: 100
        repeat: false
        onTriggered: {
            console.log("[VideoItem] 定时器触发, localVideoOutput=", localVideoOutput ? "有效" : "null", 
                        "videoSink=", (localVideoOutput && localVideoOutput.videoSink) ? "有效" : "null")
            if (isLocalUser && mediaCapture && localVideoOutput && localVideoOutput.videoSink) {
                console.log("[VideoItem] 调用 mediaCapture.bindVideoSink")
                // 【关键调用】将 VideoOutput 的 videoSink 传给 MediaCapture
                // 必须使用 bindVideoSink（Q_INVOKABLE），不能用 setVideoSink（普通方法）
                mediaCapture.bindVideoSink(localVideoOutput.videoSink)
            }
        }
    }
    
    // 视频占位符或摄像头画面
    Item {
        anchors.fill: parent
        anchors.margins: 2
        
        // 当摄像头关闭时显示头像
        Rectangle {
            anchors.fill: parent
            radius: 6
            color: "#1A1A2E"
            visible: !isCameraOn
            
            Column {
                anchors.centerIn: parent
                spacing: 8
                
                // 头像
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(parent.parent.width, parent.parent.height) * 0.3
                    height: width
                    radius: width / 2
                    color: isHost ? "#1E90FF" : "#4D4D6C"
                    
                    Text {
                        anchors.centerIn: parent
                        text: participantName.charAt(0)
                        font.pixelSize: parent.width * 0.5
                        font.bold: true
                        color: "white"
                    }
                }
            }
        }
        
        // 当摄像头开启时显示视频画面
        Rectangle {
            id: videoContainer
            anchors.fill: parent
            radius: 6
            visible: isCameraOn
            clip: true
            color: "#1A1A2E"
            
            // 本地用户显示摄像头预览
            VideoOutput {
                id: localVideoOutput
                anchors.fill: parent
                visible: isLocalUser && isCameraOn && mediaCapture !== null && mediaCapture.cameraActive
                fillMode: VideoOutput.PreserveAspectCrop
                
                // 镜像显示本地摄像头（更自然）
                transform: Scale { 
                    origin.x: localVideoOutput.width / 2
                    xScale: -1 
                }
                
                Component.onCompleted: {
                    console.log("[VideoItem] VideoOutput 初始化, isLocalUser=", isLocalUser, "participantName=", participantName)
                    if (isLocalUser && mediaCapture && localVideoOutput.videoSink) {
                        console.log("[VideoItem] 尝试绑定 VideoSink")
                        mediaCapture.bindVideoSink(localVideoOutput.videoSink)
                    }
                }
            }
            
            // 监听摄像头状态变化
            Connections {
                target: isLocalUser && mediaCapture ? mediaCapture : null
                
                function onCameraActiveChanged() {
                    if (isLocalUser && mediaCapture && mediaCapture.cameraActive && localVideoOutput.videoSink) {
                        console.log("[VideoItem] 摄像头激活，重新绑定 VideoSink")
                        mediaCapture.bindVideoSink(localVideoOutput.videoSink)
                    }
                }
            }
            
            // 远程用户显示远程视频流
            VideoOutput {
                id: remoteVideoOutput
                anchors.fill: parent
                visible: !isLocalUser && isCameraOn
                fillMode: VideoOutput.PreserveAspectCrop
                
                // 【关键】当 VideoOutput 创建完成后，将其内部 videoSink 传递给 RemoteVideoRenderer
                // VideoOutput.videoSink 是只读的，我们需要让 RemoteVideoRenderer 写入它
                Component.onCompleted: {
                    console.log("[VideoItem] 远程 VideoOutput 初始化, participantId=", participantId, "participantName=", participantName)
                    if (!isLocalUser && participantId !== "" && participantId !== "self" && remoteVideoOutput.videoSink) {
                        console.log("[VideoItem] 传递 VideoSink 给 RemoteVideoRenderer:", participantId)
                        liveKitManager.setRemoteVideoSink(participantId, remoteVideoOutput.videoSink)
                    }
                }
            }
            
            // 远程用户占位符（当没有视频流时显示一个加载指示）
            Rectangle {
                id: remotePlaceholder
                anchors.fill: parent
                visible: !isLocalUser && !isCameraOn
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#2D3748" }
                    GradientStop { position: 1.0; color: "#1A202C" }
                }
                
                Text {
                    anchors.centerIn: parent
                    text: "👤"
                    font.pixelSize: 48
                    opacity: 0.3
                }
            }
        }
    }
    
    // 底部信息栏
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 8
        height: 32
        radius: 6
        color: "#00000080"
        
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 6
            
            // 麦克风状态
            Rectangle {
                width: 20
                height: 20
                radius: 4
                color: isMicOn ? "transparent" : "#F4433680"
                
                Text {
                    anchors.centerIn: parent
                    text: isMicOn ? "🎤" : "🔇"
                    font.pixelSize: 12
                }
            }
            
            // 参会者名称
            Text {
                Layout.fillWidth: true
                text: participantName + (isHost ? " (主持人)" : "")
                font.pixelSize: 12
                color: "white"
                elide: Text.ElideRight
            }
            
            // 举手图标
            Text {
                visible: isHandRaised
                text: "✋"
                font.pixelSize: 14
                
                SequentialAnimation on scale {
                    running: isHandRaised
                    loops: Animation.Infinite
                    NumberAnimation { to: 1.2; duration: 300 }
                    NumberAnimation { to: 1.0; duration: 300 }
                }
            }
            
            // 屏幕共享图标
            Text {
                visible: isScreenSharing
                text: "🖥"
                font.pixelSize: 14
            }
        }
    }
    
    // 悬停效果
    Rectangle {
        anchors.fill: parent
        radius: 8
        color: "transparent"
        border.color: hoverArea.containsMouse ? "#1E90FF" : "transparent"
        border.width: 2
        
        Behavior on border.color {
            ColorAnimation { duration: 150 }
        }
    }
    
    MouseArea {
        id: hoverArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        
        onClicked: function(mouse) {
            if (mouse.button === Qt.RightButton) {
                contextMenu.popup()
            }
        }
        
        onDoubleClicked: {
            // 双击放大视频
        }
    }
    
    // 右键菜单
    Menu {
        id: contextMenu
        
        background: Rectangle {
            implicitWidth: 160
            color: "#252542"
            radius: 8
            border.color: "#404060"
        }
        
        MenuItem {
            text: "放大视频"
            onTriggered: {
                // 放大视频逻辑
            }
        }
        
        MenuItem {
            text: isMicOn ? "请求静音" : "请求开麦"
            enabled: isHost
            onTriggered: {
                // 静音/开麦请求
            }
        }
        
        MenuSeparator {
            contentItem: Rectangle {
                implicitHeight: 1
                color: "#404060"
            }
        }
        
        MenuItem {
            text: "设为主讲人"
            enabled: !isHost
            onTriggered: {
                // 设为主讲人
            }
        }
    }
}
