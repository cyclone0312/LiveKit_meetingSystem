import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15
import QtMultimedia

Page {
    id: homePage
    
    signal joinMeeting(string meetingId)
    signal createMeeting(string title)
    signal logout()
    
    // 连接 liveKitManager 会议历史信号
    Connections {
        target: meetingController ? meetingController.liveKitManager : null
        
        function onMeetingHistoryReceived(history) {
            recentMeetingsModel.clear()
            for (var i = 0; i < history.length; i++) {
                var meeting = history[i]
                // 格式化时长
                var durationStr = formatDuration(meeting.duration)
                // 格式化时间
                var timeStr = formatDateTime(meeting.startTime)
                recentMeetingsModel.append({
                    "historyId": meeting.id,
                    "title": meeting.roomName,
                    "meetingId": meeting.roomName,
                    "time": timeStr,
                    "duration": durationStr
                })
            }
            console.log("[HomePage] 加载了", history.length, "条会议记录")
        }
        
        function onMeetingHistoryFailed(error) {
            console.log("[HomePage] 获取会议历史失败:", error)
        }
    }
    
    // 页面加载时获取会议历史
    Component.onCompleted: {
        if (meetingController && meetingController.userName && meetingController.liveKitManager) {
            meetingController.liveKitManager.fetchMeetingHistory(meetingController.userName)
        }
    }
    
    // 格式化时长（秒转为可读字符串）
    function formatDuration(seconds) {
        if (!seconds || seconds === 0) return "未知"
        var hours = Math.floor(seconds / 3600)
        var minutes = Math.floor((seconds % 3600) / 60)
        if (hours > 0) {
            return hours + "小时" + (minutes > 0 ? minutes + "分" : "")
        } else if (minutes > 0) {
            return minutes + "分钟"
        } else {
            return seconds + "秒"
        }
    }
    
    // 格式化日期时间
    function formatDateTime(dateTimeStr) {
        if (!dateTimeStr) return "未知时间"
        // 服务器返回的格式可能是 ISO 8601
        var date = new Date(dateTimeStr)
        if (isNaN(date.getTime())) return dateTimeStr
        return Qt.formatDateTime(date, "yyyy-MM-dd hh:mm")
    }

    // 获取服务器 URL
    function getServerUrl() {
        if (meetingController && meetingController.liveKitManager)
            return meetingController.liveKitManager.tokenServerUrl
        return "http://8.162.3.195:3000"
    }

    // 检查房间状态并决定是否加入
    function checkRoomAndJoin(meetingId) {
        var xhr = new XMLHttpRequest()
        xhr.open("GET", getServerUrl() + "/api/room/status?roomName=" + encodeURIComponent(meetingId))
        xhr.onreadystatechange = function() {
            if (xhr.readyState === XMLHttpRequest.DONE) {
                if (xhr.status === 200) {
                    try {
                        var response = JSON.parse(xhr.responseText)
                        if (response.active) {
                            joinMeeting(meetingId)
                        } else {
                            roomClosedDialog.open()
                        }
                    } catch (e) {
                        console.log("[HomePage] 解析房间状态失败:", e)
                        joinMeeting(meetingId) // 解析失败时允许尝试加入
                    }
                } else {
                    console.log("[HomePage] 查询房间状态失败:", xhr.status)
                    joinMeeting(meetingId) // 网络失败时允许尝试加入
                }
            }
        }
        xhr.send()
    }

    // 删除会议历史记录
    function deleteHistoryItem(historyId, index) {
        var username = meetingController ? meetingController.userName : ""
        if (!username) return

        var xhr = new XMLHttpRequest()
        xhr.open("POST", getServerUrl() + "/api/history/delete")
        xhr.setRequestHeader("Content-Type", "application/json")
        xhr.onreadystatechange = function() {
            if (xhr.readyState === XMLHttpRequest.DONE) {
                if (xhr.status === 200) {
                    try {
                        var response = JSON.parse(xhr.responseText)
                        if (response.success) {
                            recentMeetingsModel.remove(index)
                            console.log("[HomePage] 已删除会议记录 id=" + historyId)
                        }
                    } catch (e) {
                        console.log("[HomePage] 删除解析失败:", e)
                    }
                } else {
                    console.log("[HomePage] 删除失败:", xhr.status)
                }
            }
        }
        xhr.send(JSON.stringify({ id: historyId, username: username }))
    }

    // 会议已关闭弹窗
    Dialog {
        id: roomClosedDialog
        anchors.centerIn: parent
        title: "无法加入"
        modal: true
        standardButtons: Dialog.Ok

        background: Rectangle {
            radius: 12
            color: "#252542"
            border.color: "#3D3D5C"
            border.width: 1
        }

        header: Rectangle {
            color: "transparent"
            height: 48
            Text {
                anchors.centerIn: parent
                text: "无法加入会议"
                font.pixelSize: 16
                font.bold: true
                color: "#FFFFFF"
            }
        }

        contentItem: Text {
            text: "该会议已结束或已关闭，\n无法重新加入。"
            font.pixelSize: 14
            color: "#C0C0D0"
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            padding: 20
        }

        footer: DialogButtonBox {
            alignment: Qt.AlignHCenter
            background: Rectangle { color: "transparent" }
            delegate: Button {
                text: "知道了"
                font.pixelSize: 14
                background: Rectangle {
                    radius: 6
                    color: parent.pressed ? "#1873CC" : "#1E90FF"
                }
                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }

    // 右键菜单
    Menu {
        id: contextMenu
        property int targetIndex: -1
        property int targetHistoryId: -1

        background: Rectangle {
            radius: 8
            color: "#2D2D4A"
            border.color: "#3D3D5C"
            border.width: 1
            implicitWidth: 120
        }

        MenuItem {
            text: "🗑 删除记录"
            font.pixelSize: 14

            background: Rectangle {
                radius: 4
                color: parent.highlighted ? "#3D3D5C" : "transparent"
            }

            contentItem: Text {
                text: parent.text
                font: parent.font
                color: parent.highlighted ? "#FF6B6B" : "#C0C0D0"
                verticalAlignment: Text.AlignVCenter
                leftPadding: 12
            }

            onTriggered: {
                if (contextMenu.targetIndex >= 0 && contextMenu.targetHistoryId > 0) {
                    deleteHistoryItem(contextMenu.targetHistoryId, contextMenu.targetIndex)
                }
            }
        }
    }

    background: Rectangle {
        color: "#1A1A2E"
    }
    
    // 顶部栏
    Rectangle {
        id: topBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 64
        color: "#252542"
        
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            spacing: 16
            
            // Logo
            RowLayout {
                spacing: 12
                
                Rectangle {
                    width: 36
                    height: 36
                    radius: 8
                    color: "#1E90FF"
                    
                    Text {
                        anchors.centerIn: parent
                        text: "会"
                        font.pixelSize: 18
                        font.bold: true
                        color: "white"
                    }
                }
                
                Text {
                    text: "视频会议"
                    font.pixelSize: 18
                    font.bold: true
                    color: "#FFFFFF"
                }
            }
            
            Item { Layout.fillWidth: true }
            
            // 用户信息
            RowLayout {
                spacing: 12
                
                // 用户头像
                Rectangle {
                    width: 36
                    height: 36
                    radius: 18
                    color: "#1E90FF"
                    
                    Text {
                        anchors.centerIn: parent
                        // 【关键修复】添加空值检查，避免程序关闭时崩溃
                        text: (meetingController && meetingController.userName) ? meetingController.userName.charAt(0) : ""
                        font.pixelSize: 16
                        font.bold: true
                        color: "white"
                    }
                }
                
                Text {
                    // 【关键修复】添加空值检查
                    text: (meetingController && meetingController.userName) ? meetingController.userName : ""
                    font.pixelSize: 14
                    color: "#FFFFFF"
                }
                
                // 设置按钮
                Button {
                    implicitWidth: 36
                    implicitHeight: 36
                    flat: true
                    
                    background: Rectangle {
                        radius: 18
                        color: parent.hovered ? "#3D3D5C" : "transparent"
                    }
                    
                    Text {
                        anchors.centerIn: parent
                        text: "⚙"
                        font.pixelSize: 18
                        color: "#B0B0C0"
                    }
                    
                    onClicked: settingsDialog.open()
                }
                
                // 退出按钮
                Button {
                    implicitWidth: 36
                    implicitHeight: 36
                    flat: true
                    
                    background: Rectangle {
                        radius: 18
                        color: parent.hovered ? "#3D3D5C" : "transparent"
                    }
                    
                    Text {
                        anchors.centerIn: parent
                        text: "⏻"
                        font.pixelSize: 18
                        color: "#B0B0C0"
                    }
                    
                    onClicked: logout()
                }
            }
        }
    }
    
    // 当前页面: "home" 或 "recordings"
    property string currentPage: "home"

    // 会议纪要结果弹窗内容
    property string minutesResultText: ""

    // 音频播放状态
    property string currentPlayingFile: ""

    MediaPlayer {
        id: audioPlayer
        audioOutput: AudioOutput { id: audioOutput }
        onPlaybackStateChanged: {
            if (playbackState === MediaPlayer.StoppedState) {
                currentPlayingFile = "";
            }
        }
    }

    // 主内容区
    Item {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: topBar.bottom
        anchors.bottom: parent.bottom
        
        // 左侧菜单
        Rectangle {
            id: sideMenu
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 220
            color: "#252542"

            ColumnLayout {
                anchors.fill: parent
                anchors.topMargin: 20
                spacing: 8

                // 菜单项
                Repeater {
                    model: [
                        { icon: "🏠", text: "首页", page: "home" },
                        { icon: "📅", text: "会议日程", page: "schedule" },
                        { icon: "📝", text: "历史会议", page: "history" },
                        { icon: "📁", text: "本地录制", page: "recordings" }
                    ]

                    delegate: Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 48
                        Layout.leftMargin: 12
                        Layout.rightMargin: 12
                        radius: 8
                        color: currentPage === modelData.page ? "#1E90FF" :
                               menuArea.containsMouse ? "#3D3D5C" : "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 16
                            spacing: 12

                            Text {
                                text: modelData.icon
                                font.pixelSize: 18
                            }

                            Text {
                                text: modelData.text
                                font.pixelSize: 14
                                color: currentPage === modelData.page ? "white" : "#B0B0C0"
                            }
                        }

                        MouseArea {
                            id: menuArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                currentPage = modelData.page;
                                if (modelData.page === "recordings") {
                                    aiAssistant.loadLocalRecordings();
                                }
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }
        
        // 右侧内容
        Item {
            anchors.left: sideMenu.right
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.margins: 24

            // ===== 首页内容 =====
            ColumnLayout {
                anchors.fill: parent
                spacing: 24
                visible: currentPage === "home"
                
                // 快捷操作卡片
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 200
                    radius: 16
                    color: "#252542"
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 32
                        spacing: 24
                        
                        // 快速会议
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: 12
                            color: actionArea1.containsMouse ? "#3D3D5C" : "#2D2D4A"
                            
                            Behavior on color {
                                ColorAnimation { duration: 150 }
                            }
                            
                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 12
                                
                                Rectangle {
                                    Layout.alignment: Qt.AlignHCenter
                                    width: 56
                                    height: 56
                                    radius: 28
                                    color: "#1E90FF"
                                    
                                    Text {
                                        anchors.centerIn: parent
                                        text: "📹"
                                        font.pixelSize: 24
                                    }
                                }
                                
                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: "快速会议"
                                    font.pixelSize: 16
                                    font.bold: true
                                    color: "#FFFFFF"
                                }
                                
                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: "立即开始新会议"
                                    font.pixelSize: 13
                                    color: "#808090"
                                }
                            }
                            
                            MouseArea {
                                id: actionArea1
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: createMeeting("")
                            }
                        }
                        
                        // 加入会议
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: 12
                            color: actionArea2.containsMouse ? "#3D3D5C" : "#2D2D4A"
                            
                            Behavior on color {
                                ColorAnimation { duration: 150 }
                            }
                            
                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 12
                                
                                Rectangle {
                                    Layout.alignment: Qt.AlignHCenter
                                    width: 56
                                    height: 56
                                    radius: 28
                                    color: "#4CAF50"
                                    
                                    Text {
                                        anchors.centerIn: parent
                                        text: "➕"
                                        font.pixelSize: 24
                                    }
                                }
                                
                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: "加入会议"
                                    font.pixelSize: 16
                                    font.bold: true
                                    color: "#FFFFFF"
                                }
                                
                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: "输入会议号加入"
                                    font.pixelSize: 13
                                    color: "#808090"
                                }
                            }
                            
                            MouseArea {
                                id: actionArea2
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: joinMeetingDialog.open()
                            }
                        }
                        
                        // 预约会议
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: 12
                            color: actionArea3.containsMouse ? "#3D3D5C" : "#2D2D4A"
                            
                            Behavior on color {
                                ColorAnimation { duration: 150 }
                            }
                            
                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 12
                                
                                Rectangle {
                                    Layout.alignment: Qt.AlignHCenter
                                    width: 56
                                    height: 56
                                    radius: 28
                                    color: "#FF9800"
                                    
                                    Text {
                                        anchors.centerIn: parent
                                        text: "📅"
                                        font.pixelSize: 24
                                    }
                                }
                                
                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: "预约会议"
                                    font.pixelSize: 16
                                    font.bold: true
                                    color: "#FFFFFF"
                                }
                                
                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: "安排未来会议"
                                    font.pixelSize: 13
                                    color: "#808090"
                                }
                            }
                            
                            MouseArea {
                                id: actionArea3
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: createMeetingDialog.open()
                            }
                        }
                        
                        // 共享屏幕
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: 12
                            color: actionArea4.containsMouse ? "#3D3D5C" : "#2D2D4A"
                            
                            Behavior on color {
                                ColorAnimation { duration: 150 }
                            }
                            
                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 12
                                
                                Rectangle {
                                    Layout.alignment: Qt.AlignHCenter
                                    width: 56
                                    height: 56
                                    radius: 28
                                    color: "#9C27B0"
                                    
                                    Text {
                                        anchors.centerIn: parent
                                        text: "🖥"
                                        font.pixelSize: 24
                                    }
                                }
                                
                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: "共享屏幕"
                                    font.pixelSize: 16
                                    font.bold: true
                                    color: "#FFFFFF"
                                }
                                
                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: "共享到会议"
                                    font.pixelSize: 13
                                    color: "#808090"
                                }
                            }
                            
                            MouseArea {
                                id: actionArea4
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                            }
                        }
                    }
                }
                
                // 最近会议
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 16
                    color: "#252542"
                    
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 24
                        spacing: 16
                        
                        RowLayout {
                            Layout.fillWidth: true
                            
                            Text {
                                text: "最近会议"
                                font.pixelSize: 18
                                font.bold: true
                                color: "#FFFFFF"
                            }
                            
                            Item { Layout.fillWidth: true }
                            
                            Text {
                                text: "查看全部"
                                font.pixelSize: 14
                                color: "#1E90FF"
                                
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                }
                            }
                        }
                        
                        // 会议列表
                        ListView {
                            id: recentMeetingsListView
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 8
                            
                            model: ListModel {
                                id: recentMeetingsModel
                            }
                            
                            // 加载状态
                            Text {
                                anchors.centerIn: parent
                                text: parent.count === 0 ? "暂无会议记录" : ""
                                font.pixelSize: 14
                                color: "#808090"
                                visible: parent.count === 0
                            }
                            
                            delegate: Rectangle {
                                width: ListView.view.width
                                height: 72
                                radius: 8
                                color: itemArea.containsMouse ? "#3D3D5C" : "#2D2D4A"
                                
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 16
                                    anchors.rightMargin: 16
                                    spacing: 16
                                    
                                    // 会议图标
                                    Rectangle {
                                        width: 44
                                        height: 44
                                        radius: 8
                                        color: "#1E90FF"
                                        opacity: 0.2
                                        
                                        Text {
                                            anchors.centerIn: parent
                                            text: "📹"
                                            font.pixelSize: 20
                                        }
                                    }
                                    
                                    // 会议信息
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 4
                                        
                                        Text {
                                            text: model.title
                                            font.pixelSize: 15
                                            font.bold: true
                                            color: "#FFFFFF"
                                        }
                                        
                                        Text {
                                            text: model.time + " | 时长: " + model.duration
                                            font.pixelSize: 13
                                            color: "#808090"
                                        }
                                    }
                                    
                                    // 操作按钮
                                    Button {
                                        implicitWidth: 80
                                        implicitHeight: 32
                                        text: "重新加入"
                                        font.pixelSize: 12
                                        
                                        background: Rectangle {
                                            radius: 6
                                            color: parent.pressed ? "#1873CC" : 
                                                   parent.hovered ? "#4DA6FF" : "#1E90FF"
                                        }
                                        
                                        contentItem: Text {
                                            text: parent.text
                                            font: parent.font
                                            color: "white"
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        
                                        onClicked: checkRoomAndJoin(model.meetingId)
                                    }
                                }
                                
                                MouseArea {
                                    id: itemArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    acceptedButtons: Qt.RightButton
                                    onClicked: function(mouse) {
                                        if (mouse.button === Qt.RightButton) {
                                            contextMenu.targetIndex = index
                                            contextMenu.targetHistoryId = model.historyId
                                            contextMenu.popup()
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ===== 本地录制管理面板 =====
            ColumnLayout {
                anchors.fill: parent
                spacing: 16
                visible: currentPage === "recordings"

                // 标题
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 60
                    radius: 16
                    color: "#252542"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 24
                        anchors.rightMargin: 24
                        spacing: 16

                        Text {
                            text: "📁 本地录制"
                            font.pixelSize: 20
                            font.bold: true
                            color: "#FFFFFF"
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            text: (aiAssistant ? aiAssistant.localRecordings.length : 0) + " 个录音文件"
                            font.pixelSize: 13
                            color: "#808090"
                        }
                    }
                }

                // 录音列表
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 16
                    color: "#252542"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 8

                        // 列表头部
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            color: "#2D2D4A"
                            radius: 6

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                spacing: 8

                                Text { text: "会议名称"; font.pixelSize: 12; font.bold: true; color: "#808090"; Layout.fillWidth: true }
                                Text { text: "时长"; font.pixelSize: 12; font.bold: true; color: "#808090"; Layout.preferredWidth: 60 }
                                Text { text: "大小"; font.pixelSize: 12; font.bold: true; color: "#808090"; Layout.preferredWidth: 60 }
                                Text { text: "日期"; font.pixelSize: 12; font.bold: true; color: "#808090"; Layout.preferredWidth: 130 }
                                Text { text: "操作"; font.pixelSize: 12; font.bold: true; color: "#808090"; Layout.preferredWidth: 230; horizontalAlignment: Text.AlignHCenter }
                            }
                        }

                        // 文件列表
                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 4
                            model: aiAssistant ? aiAssistant.localRecordings : []

                            // 空状态
                            Text {
                                anchors.centerIn: parent
                                text: "暂无本地录制\n\n在会议中点击录制按钮后，录音会自动保存到这里"
                                font.pixelSize: 14
                                color: "#808090"
                                visible: parent.count === 0
                                horizontalAlignment: Text.AlignHCenter
                                lineHeight: 1.5
                            }

                            delegate: Rectangle {
                                property bool isPlaying: currentPlayingFile === modelData.filePath && audioPlayer.playbackState === MediaPlayer.PlayingState
                                width: ListView.view.width
                                height: 56
                                radius: 8
                                color: isPlaying ? "#1A3A5C" : (recItemArea.containsMouse ? "#3D3D5C" : "#2D2D4A")
                                border.width: isPlaying ? 1 : 0
                                border.color: "#1E90FF"

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 16
                                    anchors.rightMargin: 16
                                    spacing: 8

                                    // 会议名称
                                    Text {
                                        text: modelData.meetingTitle || modelData.fileName
                                        font.pixelSize: 13
                                        color: "#FFFFFF"
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    // 时长
                                    Text {
                                        text: modelData.durationStr
                                        font.pixelSize: 12
                                        color: "#B0B0C0"
                                        Layout.preferredWidth: 60
                                    }

                                    // 大小
                                    Text {
                                        text: modelData.fileSizeStr
                                        font.pixelSize: 12
                                        color: "#B0B0C0"
                                        Layout.preferredWidth: 60
                                    }

                                    // 日期
                                    Text {
                                        text: modelData.dateTime
                                        font.pixelSize: 12
                                        color: "#B0B0C0"
                                        Layout.preferredWidth: 130
                                    }

                                    // 操作按钮
                                    RowLayout {
                                        Layout.preferredWidth: 230
                                        spacing: 4

                                        // 播放
                                        Button {
                                            implicitWidth: 42
                                            implicitHeight: 28
                                            background: Rectangle {
                                                radius: 6
                                                color: isPlaying ? "#FF9800" : (parent.parent.parent.parent.isPlaying ? "#FF9800" : (parent.hovered ? "#AB47BC" : "#7B1FA2"))
                                            }
                                            contentItem: Text {
                                                text: currentPlayingFile === modelData.filePath && audioPlayer.playbackState === MediaPlayer.PlayingState ? "⏸" : "▶"
                                                font.pixelSize: 14
                                                color: "white"
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                            onClicked: {
                                                if (currentPlayingFile === modelData.filePath && audioPlayer.playbackState === MediaPlayer.PlayingState) {
                                                    audioPlayer.pause();
                                                } else {
                                                    currentPlayingFile = modelData.filePath;
                                                    audioPlayer.source = "file:///" + modelData.filePath.replace(/\\\\/g, "/");
                                                    audioPlayer.play();
                                                }
                                            }
                                        }

                                        // 转录
                                        Button {
                                            implicitWidth: 52
                                            implicitHeight: 28
                                            enabled: aiAssistant ? (!aiAssistant.isTranscribing && !aiAssistant.isBusy) : false
                                            background: Rectangle {
                                                radius: 6
                                                color: parent.enabled ? (parent.hovered ? "#66BB6A" : "#4CAF50") : "#555"
                                            }
                                            contentItem: Text {
                                                text: "🎯 转录"
                                                font.pixelSize: 11
                                                color: "white"
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                            onClicked: {
                                                aiAssistant.transcribeLocalFile(modelData.filePath)
                                            }
                                        }

                                        // 生成纪要
                                        Button {
                                            implicitWidth: 52
                                            implicitHeight: 28
                                            enabled: aiAssistant ? (!aiAssistant.isTranscribing && !aiAssistant.isBusy) : false
                                            background: Rectangle {
                                                radius: 6
                                                color: parent.enabled ? (parent.hovered ? "#42A5F5" : "#1E90FF") : "#555"
                                            }
                                            contentItem: Text {
                                                text: "📋 纪要"
                                                font.pixelSize: 11
                                                color: "white"
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                            onClicked: {
                                                aiAssistant.generateMinutesFromFile(modelData.filePath)
                                            }
                                        }

                                        // 删除
                                        Button {
                                            implicitWidth: 52
                                            implicitHeight: 28
                                            background: Rectangle {
                                                radius: 6
                                                color: parent.hovered ? "#EF5350" : "#3D3D5C"
                                            }
                                            contentItem: Text {
                                                text: "🗑 删除"
                                                font.pixelSize: 11
                                                color: parent.hovered ? "white" : "#B0B0C0"
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                            onClicked: {
                                                aiAssistant.deleteLocalRecording(index)
                                            }
                                        }
                                    }
                                }

                                MouseArea {
                                    id: recItemArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    acceptedButtons: Qt.NoButton
                                }
                            }
                        }

                        // 播放控制栏
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: currentPlayingFile !== "" ? 48 : 0
                            visible: currentPlayingFile !== ""
                            color: "#1A2A4A"
                            radius: 8
                            clip: true

                            Behavior on Layout.preferredHeight {
                                NumberAnimation { duration: 200 }
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                spacing: 12

                                Text {
                                    text: audioPlayer.playbackState === MediaPlayer.PlayingState ? "🔊 正在播放" : "⏸ 已暂停"
                                    font.pixelSize: 12
                                    color: "#4FC3F7"
                                }

                                // 进度条
                                Slider {
                                    Layout.fillWidth: true
                                    from: 0
                                    to: audioPlayer.duration > 0 ? audioPlayer.duration : 1
                                    value: audioPlayer.position
                                    onMoved: audioPlayer.position = value

                                    background: Rectangle {
                                        x: parent.leftPadding
                                        y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                        width: parent.availableWidth
                                        height: 4
                                        radius: 2
                                        color: "#3D3D5C"

                                        Rectangle {
                                            width: parent.parent.visualPosition * parent.width
                                            height: parent.height
                                            radius: 2
                                            color: "#1E90FF"
                                        }
                                    }

                                    handle: Rectangle {
                                        x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
                                        y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                        width: 12
                                        height: 12
                                        radius: 6
                                        color: "#1E90FF"
                                    }
                                }

                                // 时间显示
                                Text {
                                    property int pos: Math.floor(audioPlayer.position / 1000)
                                    property int dur: Math.floor(audioPlayer.duration / 1000)
                                    text: String(Math.floor(pos/60)).padStart(2,'0') + ":" + String(pos%60).padStart(2,'0') + " / " + String(Math.floor(dur/60)).padStart(2,'0') + ":" + String(dur%60).padStart(2,'0')
                                    font.pixelSize: 11
                                    color: "#B0B0C0"
                                }

                                // 停止按钮
                                Button {
                                    implicitWidth: 28
                                    implicitHeight: 28
                                    background: Rectangle {
                                        radius: 6
                                        color: parent.hovered ? "#EF5350" : "#3D3D5C"
                                    }
                                    contentItem: Text {
                                        text: "⏹"
                                        font.pixelSize: 14
                                        color: "white"
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    onClicked: {
                                        audioPlayer.stop();
                                        currentPlayingFile = "";
                                    }
                                }
                            }
                        }

                        // 处理中提示
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: (aiAssistant && (aiAssistant.isTranscribing || aiAssistant.isBusy)) ? 40 : 0
                            visible: aiAssistant ? (aiAssistant.isTranscribing || aiAssistant.isBusy) : false
                            color: "#1E2A2E"
                            radius: 8
                            clip: true

                            RowLayout {
                                anchors.centerIn: parent
                                spacing: 8

                                BusyIndicator {
                                    Layout.preferredWidth: 16
                                    Layout.preferredHeight: 16
                                    running: true
                                }

                                Text {
                                    text: (aiAssistant && aiAssistant.isTranscribing) ? "正在转录中..." : "正在生成会议纪要..."
                                    font.pixelSize: 12
                                    color: "#4FC3F7"
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // 会议纪要结果弹窗
    Dialog {
        id: minutesDialog
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.7, 600)
        height: Math.min(parent.height * 0.7, 500)
        title: "会议纪要"
        modal: true
        standardButtons: Dialog.Close

        background: Rectangle {
            radius: 12
            color: "#252542"
            border.color: "#3D3D5C"
            border.width: 1
        }

        header: Rectangle {
            color: "transparent"
            height: 48
            Text {
                anchors.centerIn: parent
                text: "📋 会议纪要"
                font.pixelSize: 16
                font.bold: true
                color: "#FFFFFF"
            }
        }

        contentItem: Flickable {
            contentHeight: minutesText.implicitHeight
            clip: true

            Text {
                id: minutesText
                width: parent.width
                text: minutesResultText
                font.pixelSize: 13
                color: "#D0D0E0"
                wrapMode: Text.Wrap
                lineHeight: 1.4
                padding: 16
            }
        }

        footer: DialogButtonBox {
            alignment: Qt.AlignHCenter
            background: Rectangle { color: "transparent" }
            delegate: Button {
                text: "关闭"
                font.pixelSize: 14
                background: Rectangle {
                    radius: 6
                    color: parent.pressed ? "#1873CC" : "#1E90FF"
                }
                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }

    // AI 信号连接
    Connections {
        target: aiAssistant

        function onMinutesFromFileReceived(minutes) {
            minutesResultText = minutes
            minutesDialog.open()
        }

        function onFileTranscriptionCompleted(filePath, transcript) {
            console.log("[HomePage] 文件转录完成: " + filePath)
        }
    }
}
