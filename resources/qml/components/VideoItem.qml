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

    signal toggleFocusRequested(string tileId)
    
    property string tileId: ""
    property string participantId: ""  // 参会者 ID
    property string participantName: ""
    property string streamType: "camera"
    property string streamLabel: "摄像头"
    property string remoteRenderKey: ""
    property bool isMicOn: false
    property bool isHost: false
    property bool isHandRaised: false
    property bool isStreamActive: false
    property bool isLocalUser: false  // 是否是本地用户
    property bool isFocused: false
    property string lastBoundRenderKey: ""
    property var lastBoundRemoteSink: null
    readonly property bool isScreenTile: streamType === "screen"
    readonly property int streamSource: isScreenTile ? 3 : 1
    
    // 【重要】MediaCapture 对象引用，必须从父组件传入
    // 在 GridView 的 delegate 中无法直接访问全局变量，会得到 null
    // 需要通过 VideoGrid 的属性中转传递
    property var mediaCapture: null
    
    // 【新增】ScreenCapture 对象引用，用于本地屏幕共享预览
    property var screenCapture: null
    
    radius: 8
    color: "#252542"
    border.color: isScreenTile ? "#1E90FF" : "transparent"
    border.width: isScreenTile ? 2 : 0
    
    // 【关键】组件销毁时停止所有定时器，防止定时器在组件销毁后触发导致崩溃
    Component.onDestruction: {
        bindVideoSinkTimer.stop()
        bindScreenShareVideoSinkTimer.stop()
        bindRemoteVideoSinkTimer.stop()
        releaseRemoteVideoSink()
    }

    function scheduleLocalVideoBinding() {
        if (!isLocalUser || !isStreamActive) {
            return
        }

        if (isScreenTile) {
            bindScreenShareVideoSinkTimer.schedule()
        } else {
            bindVideoSinkTimer.schedule()
        }
    }

    function bindRemoteVideoSink() {
        if (isLocalUser || remoteRenderKey === "" || !remoteVideoOutput || !remoteVideoOutput.videoSink) {
            return
        }

        liveKitManager.setRemoteVideoSink(remoteRenderKey, remoteVideoOutput.videoSink)
        lastBoundRenderKey = remoteRenderKey
        lastBoundRemoteSink = remoteVideoOutput.videoSink
    }

    function releaseRemoteVideoSink() {
        if (isLocalUser || lastBoundRenderKey === "") {
            return
        }

        if (lastBoundRemoteSink) {
            liveKitManager.clearRemoteVideoSinkIfMatches(lastBoundRenderKey, lastBoundRemoteSink)
        }
        lastBoundRenderKey = ""
        lastBoundRemoteSink = null
    }
    
    // 监听本地流状态变化（用户点击开启摄像头）
    onIsStreamActiveChanged: {
        console.log("[VideoItem] onIsStreamActiveChanged: tileId=", tileId, "isLocalUser=", isLocalUser, "isStreamActive=", isStreamActive)
        if (isLocalUser && !isScreenTile && isStreamActive && mediaCapture) {
            console.log("[VideoItem] isCameraOn 变为 true，准备绑定 VideoSink")
            // 延迟绑定，确保 VideoOutput 已经准备好
            scheduleLocalVideoBinding()
        } else if (isLocalUser && isScreenTile && isStreamActive && screenCapture) {
            console.log("[VideoItem] 屏幕共享变为 true，准备绑定 VideoSink")
            scheduleLocalVideoBinding()
        } else if (!isLocalUser && !isStreamActive) {
            releaseRemoteVideoSink()
        }
    }
    
    // 监听全局摄像头状态变化（针对本地用户）
    Connections {
        target: isLocalUser && !isScreenTile ? meetingController : null
        
        function onCameraOnChanged() {
            console.log("[VideoItem] meetingController.cameraOnChanged: isCameraOn=", meetingController.isCameraOn)
            if (isLocalUser && meetingController.isCameraOn && mediaCapture) {
                console.log("[VideoItem] 通过 Connections 检测到摄像头开启，准备绑定 VideoSink")
                scheduleLocalVideoBinding()
            }
        }
    }
    
    // 定时器用于延迟绑定
    // 【为什么需要延迟】VideoOutput 创建后，其 videoSink 属性可能还未初始化完成
    Timer {
        id: bindVideoSinkTimer
        interval: 100
        repeat: true
        property int attemptsRemaining: 0

        function schedule() {
            attemptsRemaining = 8
            restart()
        }

        onTriggered: {
            console.log("[VideoItem] 定时器触发, localVideoOutput=", localVideoOutput ? "有效" : "null", 
                        "videoSink=", (localVideoOutput && localVideoOutput.videoSink) ? "有效" : "null")
            if (isLocalUser && !isScreenTile && mediaCapture && localVideoOutput && localVideoOutput.videoSink) {
                console.log("[VideoItem] 调用 mediaCapture.bindVideoSink")
                // 【关键调用】将 VideoOutput 的 videoSink 传给 MediaCapture
                // 必须使用 bindVideoSink（Q_INVOKABLE），不能用 setVideoSink（普通方法）
                mediaCapture.bindVideoSink(localVideoOutput.videoSink)
                stop()
                return
            }

            attemptsRemaining--
            if (attemptsRemaining <= 0) {
                stop()
            }
        }
    }

    Timer {
        id: bindScreenShareVideoSinkTimer
        interval: 100
        repeat: true
        property int attemptsRemaining: 0

        function schedule() {
            attemptsRemaining = 8
            restart()
        }

        onTriggered: {
            if (isLocalUser && isScreenTile && screenCapture && screenShareVideoOutput && screenShareVideoOutput.videoSink) {
                console.log("[VideoItem] 调用 screenCapture.bindVideoSink")
                screenCapture.bindVideoSink(screenShareVideoOutput.videoSink)
                stop()
                return
            }

            attemptsRemaining--
            if (attemptsRemaining <= 0) {
                stop()
            }
        }
    }
    
    // 视频占位符或摄像头画面
    Item {
        anchors.fill: parent
        anchors.margins: 2
        
        // 当当前流未开启时显示头像
        Rectangle {
            anchors.fill: parent
            radius: 6
            color: "#1A1A2E"
            visible: !isStreamActive
            
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
        
        // 当前流开启时显示视频画面
        Rectangle {
            id: videoContainer
            anchors.fill: parent
            radius: 6
            visible: isStreamActive
            clip: true
            color: "#1A1A2E"
            
            // 本地用户显示摄像头预览
            VideoOutput {
                id: localVideoOutput
                anchors.fill: parent
                visible: isLocalUser && !isScreenTile && isStreamActive && mediaCapture !== null && mediaCapture.cameraActive
                fillMode: VideoOutput.PreserveAspectCrop
                
                // 镜像显示本地摄像头（更自然）
                transform: Scale { 
                    origin.x: localVideoOutput.width / 2
                    xScale: -1 
                }
                
                Component.onCompleted: {
                    console.log("[VideoItem] VideoOutput 初始化, isLocalUser=", isLocalUser, "participantName=", participantName)
                    videoItem.scheduleLocalVideoBinding()
                }

                onVisibleChanged: {
                    if (visible) {
                        videoItem.scheduleLocalVideoBinding()
                    }
                }
            }
            
            // 监听摄像头状态变化
            Connections {
                target: isLocalUser && !isScreenTile && mediaCapture ? mediaCapture : null
                
                function onCameraActiveChanged() {
                    if (isLocalUser && !isScreenTile && mediaCapture && mediaCapture.cameraActive) {
                        console.log("[VideoItem] 摄像头激活，重新绑定 VideoSink")
                        videoItem.scheduleLocalVideoBinding()
                    }
                }
            }
            
            // 【新增】本地用户屏幕共享预览
            VideoOutput {
                id: screenShareVideoOutput
                anchors.fill: parent
                visible: isLocalUser && isScreenTile && isStreamActive && screenCapture !== null && screenCapture.isActive
                fillMode: VideoOutput.PreserveAspectFit
                
                Component.onCompleted: {
                    console.log("[VideoItem] 屏幕共享 VideoOutput 初始化")
                    videoItem.scheduleLocalVideoBinding()
                }

                onVisibleChanged: {
                    if (visible) {
                        videoItem.scheduleLocalVideoBinding()
                    }
                }
            }
            
            // 监听屏幕共享状态变化
            Connections {
                target: isLocalUser && isScreenTile && screenCapture ? screenCapture : null
                
                function onActiveChanged() {
                    if (isLocalUser && isScreenTile && screenCapture && screenCapture.isActive) {
                        console.log("[VideoItem] 屏幕共享激活，绑定 VideoSink")
                        videoItem.scheduleLocalVideoBinding()
                    }
                }
            }
            
            // 远程用户显示远程视频流
            VideoOutput {
                id: remoteVideoOutput
                anchors.fill: parent
                visible: !isLocalUser && isStreamActive
                fillMode: isScreenTile ? VideoOutput.PreserveAspectFit : VideoOutput.PreserveAspectCrop
                
                // 【关键】当 VideoOutput 创建完成后，将其内部 videoSink 传递给 RemoteVideoRenderer
                // VideoOutput.videoSink 是只读的，我们需要让 RemoteVideoRenderer 写入它
                Component.onCompleted: {
                    console.log("[VideoItem] 远程 VideoOutput 初始化, tileId=", tileId, "participantId=", participantId)
                    bindRemoteVideoSinkTimer.start()
                }
            }
            
                    // 【修复3】监听 participantId 变化，重新绑定 VideoSink
            // GridView 的 delegate 可能在创建时 participantId 还未设置
            Connections {
                target: videoItem
                function onParticipantIdChanged() {
                    if (!isLocalUser && remoteRenderKey !== "") {
                        console.log("[VideoItem] participantId 变化，重新绑定 VideoSink:", remoteRenderKey)
                        bindRemoteVideoSinkTimer.start()
                    }
                }

                // 当远端当前路视频重新开启时，重新绑定 VideoSink。
                // VideoOutput 此时从不可见变为可见但 VideoSink 尚未绑定，
                // 必须触发一次重新绑定，否则视频始终黑屏。
                function onIsStreamActiveChanged() {
                    if (!isLocalUser && isStreamActive && remoteRenderKey !== "") {
                        console.log("[VideoItem] 远端流变为开启，重新绑定 VideoSink:", remoteRenderKey)
                        bindRemoteVideoSinkTimer.start()
                    }
                }

                function onRemoteRenderKeyChanged() {
                    if (!isLocalUser && lastBoundRenderKey !== "" && lastBoundRenderKey !== remoteRenderKey) {
                        releaseRemoteVideoSink()
                    }
                    if (!isLocalUser && remoteRenderKey !== "") {
                        console.log("[VideoItem] remoteRenderKey 变化，重新绑定 VideoSink:", remoteRenderKey)
                        bindRemoteVideoSinkTimer.start()
                    }
                }
            }

            // 【修复4】监听 LiveKit trackSubscribed 信号。
            // 场景：SDK 异步创建 renderer（QueuedConnection），
            // QML 组件的 Component.onCompleted 50ms 定时器可能在
            // renderer 尚未创建时就触发，sink 会进入 pendingVideoSinks。
            // 当 trackSubscribed 信号发出时 renderer 即将被创建，
            // 再延迟 200ms 后重新尝试绑定，确保两种时序都能覆盖。
            Connections {
                target: liveKitManager
                function onTrackSubscribed(identity, trackSid, trackKind, trackSource) {
                    // trackKind == 2 是视频轨道（livekit::TrackKind::KIND_VIDEO）
                    if (!isLocalUser && identity === participantId && trackKind === 2 && trackSource === streamSource) {
                        console.log("[VideoItem] trackSubscribed 事件，延迟重新绑定 VideoSink:", remoteRenderKey)
                        bindRemoteVideoSinkTimer.restart()
                    }
                }
            }

            // 【修复4】延迟绑定远程 VideoSink 的定时器
            // interval 调整为 200ms：SDK onTrackSubscribed 回调 → Qt QueuedConnection
            // 排队到主线程 → renderer 创建完成，至少需要 1-2 个事件循环周期；
            // 200ms 足以覆盖正常情况，同时 pendingVideoSinks 也作为兜底机制。
            Timer {
                id: bindRemoteVideoSinkTimer
                interval: 200
                repeat: false
                onTriggered: {
                    if (!isLocalUser && remoteRenderKey !== "" && remoteVideoOutput.videoSink) {
                        console.log("[VideoItem] 传递 VideoSink 给 RemoteVideoRenderer:", remoteRenderKey)
                        bindRemoteVideoSink()
                    }
                }
            }
            
            // 远程用户占位符（当没有视频流时显示一个加载指示）
            Rectangle {
                id: remotePlaceholder
                anchors.fill: parent
                visible: !isLocalUser && !isStreamActive
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
                text: participantName + (isScreenTile ? " · " + streamLabel : "") + (isHost ? " (主持人)" : "")
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
                visible: isScreenTile
                text: "🖥"
                font.pixelSize: 14
            }
        }
    }

    Rectangle {
        id: focusButton
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 10
        width: 34
        height: 34
        radius: 17
        color: isFocused ? "#1E90FFCC" : "#00000088"
        border.color: isFocused ? "#8FC8FF" : "#FFFFFF33"
        border.width: 1
        z: 3

        Text {
            anchors.centerIn: parent
            text: isFocused ? "🗗" : "⛶"
            font.pixelSize: 15
            color: "white"
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onClicked: function(mouse) {
                mouse.accepted = true
                videoItem.toggleFocusRequested(videoItem.tileId)
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
            videoItem.toggleFocusRequested(videoItem.tileId)
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
            text: isFocused ? "取消放大" : "放大视频"
            onTriggered: {
                videoItem.toggleFocusRequested(videoItem.tileId)
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
