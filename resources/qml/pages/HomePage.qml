import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15

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
                        { icon: "🏠", text: "首页", active: true },
                        { icon: "📅", text: "会议日程", active: false },
                        { icon: "📝", text: "历史会议", active: false },
                        { icon: "👥", text: "通讯录", active: false }
                    ]
                    
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 48
                        Layout.leftMargin: 12
                        Layout.rightMargin: 12
                        radius: 8
                        color: modelData.active ? "#1E90FF" : 
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
                                color: modelData.active ? "white" : "#B0B0C0"
                            }
                        }
                        
                        MouseArea {
                            id: menuArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
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
            
            ColumnLayout {
                anchors.fill: parent
                spacing: 24
                
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
        }
    }
}
