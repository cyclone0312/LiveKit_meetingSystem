import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Dialog {
    id: scheduleMeetingDialog

    anchors.centerIn: parent
    width: 520
    height: 580
    modal: true
    standardButtons: Dialog.NoButton

    signal scheduleCreated()

    // 当前选中的年/月/日
    property int selectedYear: new Date().getFullYear()
    property int selectedMonth: new Date().getMonth()  // 0-based
    property int selectedDay: new Date().getDate()
    property string selectedTime: {
        var h = new Date().getHours()
        var m = Math.ceil(new Date().getMinutes() / 15) * 15
        if (m >= 60) { h++; m = 0 }
        if (h >= 24) h = 23
        return String(h).padStart(2, '0') + ":" + String(m % 60).padStart(2, '0')
    }

    // 日历显示的年月（可以翻页浏览不同月份）
    property int viewYear: selectedYear
    property int viewMonth: selectedMonth

    // 获取某月有多少天
    function daysInMonth(year, month) {
        return new Date(year, month + 1, 0).getDate()
    }

    // 获取某月第一天是星期几 (0=日, 1=一, ...)
    function firstDayOfMonth(year, month) {
        return new Date(year, month, 1).getDay()
    }

    // 星期名称
    readonly property var weekDays: ["日", "一", "二", "三", "四", "五", "六"]

    // 生成时间列表 (每15分钟一档)
    function generateTimeList() {
        var list = []
        for (var h = 0; h < 24; h++) {
            for (var m = 0; m < 60; m += 15) {
                list.push(String(h).padStart(2, '0') + ":" + String(m).padStart(2, '0'))
            }
        }
        return list
    }

    readonly property var timeList: generateTimeList()

    background: Rectangle {
        color: "#252542"
        radius: 16
        border.color: "#3D3D5C"
        border.width: 1
    }

    header: Rectangle {
        width: parent.width
        height: 56
        color: "transparent"

        Text {
            anchors.centerIn: parent
            text: "预定会议"
            font.pixelSize: 20
            font.bold: true
            color: "#FFFFFF"
        }

        Button {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 12
            width: 32
            height: 32
            flat: true

            background: Rectangle {
                radius: 16
                color: parent.hovered ? "#3D3D5C" : "transparent"
            }

            Text {
                anchors.centerIn: parent
                text: "✕"
                font.pixelSize: 14
                color: "#B0B0C0"
            }

            onClicked: scheduleMeetingDialog.close()
        }
    }

    function getServerUrl() {
        if (meetingController && meetingController.liveKitManager)
            return meetingController.liveKitManager.tokenServerUrl
        return "http://8.162.3.195:3000"
    }

    // 存储最近预定的会议信息，用于成功弹窗
    property string lastScheduledTitle: ""
    property string lastScheduledRoomId: ""
    property string lastScheduledStartTime: ""
    property int lastScheduledDuration: 60

    function submitSchedule() {
        var username = meetingController ? meetingController.userName : ""
        if (!username) { statusText.text = "请先登录"; statusText.color = "#FF6B6B"; return }
        var title = titleField.text.trim()
        if (!title) { statusText.text = "请输入会议主题"; statusText.color = "#FF6B6B"; return }

        var month = String(selectedMonth + 1).padStart(2, '0')
        var day = String(selectedDay).padStart(2, '0')
        var startTime = selectedYear + "-" + month + "-" + day + " " + selectedTime + ":00"

        var selectedDate = new Date(selectedYear, selectedMonth, selectedDay,
            parseInt(selectedTime.split(":")[0]), parseInt(selectedTime.split(":")[1]))
        if (selectedDate <= new Date()) {
            statusText.text = "开始时间必须在当前时间之后"
            statusText.color = "#FF6B6B"
            return
        }

        var durationMap = [30, 60, 90, 120, 180]
        var durationMinutes = durationMap[durationCombo.currentIndex] || 60

        submitBtn.enabled = false
        statusText.text = "正在预定..."
        statusText.color = "#1E90FF"

        var readableTime = selectedYear + "/" + (selectedMonth + 1) + "/" + selectedDay + " " + selectedTime

        var xhr = new XMLHttpRequest()
        xhr.open("POST", getServerUrl() + "/api/schedule/create")
        xhr.setRequestHeader("Content-Type", "application/json")
        xhr.onreadystatechange = function() {
            if (xhr.readyState === XMLHttpRequest.DONE) {
                submitBtn.enabled = true
                if (xhr.status === 200) {
                    try {
                        var response = JSON.parse(xhr.responseText)
                        if (response.success) {
                            statusText.text = ""
                            lastScheduledTitle = title
                            lastScheduledRoomId = response.roomId
                            lastScheduledStartTime = readableTime
                            lastScheduledDuration = durationMinutes
                            successOverlay.visible = true
                            scheduleCreated()
                        } else {
                            statusText.text = "预定失败: " + (response.error || "未知错误")
                            statusText.color = "#FF6B6B"
                        }
                    } catch (e) {
                        statusText.text = "响应解析失败"
                        statusText.color = "#FF6B6B"
                    }
                } else {
                    statusText.text = "网络错误 (" + xhr.status + ")"
                    statusText.color = "#FF6B6B"
                }
            }
        }
        xhr.send(JSON.stringify({
            username: username, title: title, startTime: startTime,
            durationMinutes: durationMinutes, muteOnJoin: muteOnJoinCheck.checked,
            autoRecord: recordCheck.checked
        }))
    }

    // 成功弹出遮罩层
    Rectangle {
        id: successOverlay
        visible: false
        anchors.fill: parent
        color: "#252542"
        radius: 16
        z: 100

        ColumnLayout {
            anchors.centerIn: parent
            width: parent.width - 60
            spacing: 16

            // 成功图标
            Text {
                text: "✅"
                font.pixelSize: 36
                Layout.alignment: Qt.AlignHCenter
            }

            Text {
                text: "会议预定成功"
                font.pixelSize: 20
                font.bold: true
                color: "#4CAF50"
                Layout.alignment: Qt.AlignHCenter
            }

            // 会议信息卡片
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: infoColumn.implicitHeight + 32
                radius: 12
                color: "#1A1A2E"
                border.color: "#404060"
                border.width: 1

                ColumnLayout {
                    id: infoColumn
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 10

                    RowLayout {
                        spacing: 8
                        Text { text: "会议主题:"; font.pixelSize: 13; color: "#808090"; Layout.preferredWidth: 65 }
                        Text { text: lastScheduledTitle; font.pixelSize: 13; font.bold: true; color: "#FFFFFF"; Layout.fillWidth: true; elide: Text.ElideRight }
                    }
                    RowLayout {
                        spacing: 8
                        Text { text: "会 议 号:"; font.pixelSize: 13; color: "#808090"; Layout.preferredWidth: 65 }
                        Text { text: lastScheduledRoomId; font.pixelSize: 13; font.bold: true; color: "#1E90FF"; Layout.fillWidth: true }
                    }
                    RowLayout {
                        spacing: 8
                        Text { text: "开始时间:"; font.pixelSize: 13; color: "#808090"; Layout.preferredWidth: 65 }
                        Text { text: lastScheduledStartTime; font.pixelSize: 13; color: "#FFFFFF"; Layout.fillWidth: true }
                    }
                    RowLayout {
                        spacing: 8
                        Text { text: "会议时长:"; font.pixelSize: 13; color: "#808090"; Layout.preferredWidth: 65 }
                        Text { text: lastScheduledDuration + "分钟"; font.pixelSize: 13; color: "#FFFFFF"; Layout.fillWidth: true }
                    }
                }
            }

            // 复制按钮
            Button {
                id: copyInfoBtn
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                text: "📋 复制会议信息"

                background: Rectangle {
                    radius: 8
                    color: parent.pressed ? "#1873CC" :
                           parent.hovered ? "#4DA6FF" : "#1E90FF"
                    Behavior on color { ColorAnimation { duration: 150 } }
                }

                contentItem: Text {
                    text: parent.text
                    font.pixelSize: 14
                    font.bold: true
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: {
                    var info = "📅 会议邀请\n"
                    info += "会议主题: " + lastScheduledTitle + "\n"
                    info += "会议号: " + lastScheduledRoomId + "\n"
                    info += "开始时间: " + lastScheduledStartTime + "\n"
                    info += "会议时长: " + lastScheduledDuration + "分钟\n"
                    info += "加入方式: 打开视频会议APP → 加入会议 → 输入会议号 " + lastScheduledRoomId
                    meetingController.copyToClipboard(info)
                    copyInfoBtn.text = "✅ 已复制到剪贴板"
                    copiedResetTimer.start()
                }
            }

            Timer {
                id: copiedResetTimer
                interval: 2000
                onTriggered: copyInfoBtn.text = "📋 复制会议信息"
            }

            // 关闭按钮
            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                text: "关闭"

                background: Rectangle {
                    radius: 8
                    color: parent.pressed ? "#2D2D4C" :
                           parent.hovered ? "#4D4D6C" : "#3D3D5C"
                }

                contentItem: Text {
                    text: parent.text
                    font.pixelSize: 14
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: {
                    successOverlay.visible = false
                    scheduleMeetingDialog.close()
                }
            }
        }
    }

    contentItem: Flickable {
        contentHeight: contentColumn.implicitHeight + 16
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: contentColumn
            width: parent.width
            spacing: 20

            // ===== 主题 =====
            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                Text {
                    text: "主题"
                    font.pixelSize: 14
                    font.bold: true
                    color: "#B0B0C0"
                    Layout.preferredWidth: 48
                }

                TextField {
                    id: titleField
                    Layout.fillWidth: true
                    placeholderText: "请输入会议主题"
                    text: meetingController ? meetingController.userName + "的会议" : "我的会议"
                    font.pixelSize: 14
                    leftPadding: 12
                    rightPadding: 12
                    color: "#FFFFFF"
                    placeholderTextColor: "#606080"

                    background: Rectangle {
                        implicitHeight: 40
                        radius: 6
                        color: "#1A1A2E"
                        border.color: titleField.activeFocus ? "#1E90FF" : "#404060"
                        border.width: 1
                    }
                }
            }

            // ===== 开始时间 =====
            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                Text {
                    text: "开始"
                    font.pixelSize: 14
                    font.bold: true
                    color: "#B0B0C0"
                    Layout.preferredWidth: 48
                }

                // 日期显示按钮
                Rectangle {
                    Layout.fillWidth: true
                    height: 40
                    radius: 6
                    color: "#1A1A2E"
                    border.color: calendarPopup.visible ? "#1E90FF" : "#404060"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12

                        Text {
                            text: selectedYear + "/" + (selectedMonth + 1) + "/" + selectedDay + " " + getDayName()
                            font.pixelSize: 14
                            color: "#FFFFFF"
                            Layout.fillWidth: true

                            function getDayName() {
                                var d = new Date(selectedYear, selectedMonth, selectedDay)
                                var names = ["周日", "周一", "周二", "周三", "周四", "周五", "周六"]
                                return names[d.getDay()]
                            }
                        }

                        Text {
                            text: "📅"
                            font.pixelSize: 14
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            calendarPopup.visible = !calendarPopup.visible
                            timePopup.visible = false
                        }
                    }
                }

                // 时间选择按钮
                Rectangle {
                    Layout.preferredWidth: 100
                    height: 40
                    radius: 6
                    color: "#1A1A2E"
                    border.color: timePopup.visible ? "#1E90FF" : "#404060"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 8

                        Text {
                            text: selectedTime
                            font.pixelSize: 14
                            color: "#FFFFFF"
                            Layout.fillWidth: true
                        }

                        Text {
                            text: "▼"
                            font.pixelSize: 8
                            color: "#808090"
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            timePopup.visible = !timePopup.visible
                            calendarPopup.visible = false
                        }
                    }
                }
            }

            // ===== 日历弹出区域 =====
            Rectangle {
                id: calendarPopup
                Layout.fillWidth: true
                visible: false
                implicitHeight: calendarColumn.implicitHeight + 24
                radius: 12
                color: "#1A1A2E"
                border.color: "#404060"
                border.width: 1

                ColumnLayout {
                    id: calendarColumn
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    // 月份导航
                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            text: viewYear + "年 " + (viewMonth + 1) + "月"
                            font.pixelSize: 15
                            font.bold: true
                            color: "#FFFFFF"
                            Layout.fillWidth: true
                        }

                        Button {
                            implicitWidth: 32
                            implicitHeight: 28
                            flat: true
                            background: Rectangle {
                                radius: 6
                                color: parent.hovered ? "#3D3D5C" : "transparent"
                            }
                            contentItem: Text {
                                text: "‹"
                                font.pixelSize: 18
                                color: "#B0B0C0"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: {
                                if (viewMonth === 0) { viewMonth = 11; viewYear-- }
                                else viewMonth--
                            }
                        }

                        Button {
                            implicitWidth: 32
                            implicitHeight: 28
                            flat: true
                            background: Rectangle {
                                radius: 6
                                color: parent.hovered ? "#3D3D5C" : "transparent"
                            }
                            contentItem: Text {
                                text: "›"
                                font.pixelSize: 18
                                color: "#B0B0C0"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: {
                                if (viewMonth === 11) { viewMonth = 0; viewYear++ }
                                else viewMonth++
                            }
                        }
                    }

                    // 星期标题行
                    Row {
                        Layout.fillWidth: true
                        spacing: 0

                        Repeater {
                            model: weekDays
                            delegate: Text {
                                width: (calendarColumn.width - 24) / 7
                                text: modelData
                                font.pixelSize: 12
                                color: "#808090"
                                horizontalAlignment: Text.AlignHCenter
                                height: 28
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }

                    // 日期网格
                    Grid {
                        Layout.fillWidth: true
                        columns: 7
                        spacing: 0

                        // 前置空白
                        Repeater {
                            model: firstDayOfMonth(viewYear, viewMonth)
                            delegate: Item {
                                width: (calendarColumn.width - 24) / 7
                                height: 36
                            }
                        }

                        // 日期
                        Repeater {
                            model: daysInMonth(viewYear, viewMonth)
                            delegate: Rectangle {
                                property int dayNum: index + 1
                                property bool isSelected: viewYear === selectedYear && viewMonth === selectedMonth && dayNum === selectedDay
                                property bool isToday: {
                                    var now = new Date()
                                    return viewYear === now.getFullYear() && viewMonth === now.getMonth() && dayNum === now.getDate()
                                }
                                property bool isPast: {
                                    var d = new Date(viewYear, viewMonth, dayNum)
                                    var today = new Date()
                                    today.setHours(0,0,0,0)
                                    return d < today
                                }

                                width: (calendarColumn.width - 24) / 7
                                height: 36
                                radius: 18
                                color: isSelected ? "#1E90FF" :
                                       dayMouseArea.containsMouse && !isPast ? "#3D3D5C" : "transparent"

                                Text {
                                    anchors.centerIn: parent
                                    text: dayNum
                                    font.pixelSize: 14
                                    font.bold: isSelected || isToday
                                    color: isPast ? "#404060" :
                                           isSelected ? "white" :
                                           isToday ? "#1E90FF" : "#D0D0E0"
                                }

                                MouseArea {
                                    id: dayMouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: isPast ? Qt.ArrowCursor : Qt.PointingHandCursor
                                    onClicked: {
                                        if (!isPast) {
                                            selectedYear = viewYear
                                            selectedMonth = viewMonth
                                            selectedDay = dayNum
                                            calendarPopup.visible = false
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ===== 时间选择弹出区域 =====
            Rectangle {
                id: timePopup
                Layout.fillWidth: true
                visible: false
                height: 200
                radius: 12
                color: "#1A1A2E"
                border.color: "#404060"
                border.width: 1

                ListView {
                    id: timeListView
                    anchors.fill: parent
                    anchors.margins: 6
                    clip: true
                    model: timeList
                    spacing: 2

                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 32
                        radius: 6
                        color: modelData === selectedTime ? "#1E3A5C" :
                               timeItemArea.containsMouse ? "#2D2D4A" : "transparent"

                        Text {
                            anchors.centerIn: parent
                            text: modelData
                            font.pixelSize: 14
                            color: modelData === selectedTime ? "#1E90FF" : "#D0D0E0"
                            font.bold: modelData === selectedTime
                        }

                        MouseArea {
                            id: timeItemArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                selectedTime = modelData
                                timePopup.visible = false
                            }
                        }
                    }

                    Component.onCompleted: {
                        // 滚动到当前选中时间附近
                        var idx = timeList.indexOf(selectedTime)
                        if (idx >= 0) positionViewAtIndex(Math.max(0, idx - 3), ListView.Beginning)
                    }
                }
            }

            // ===== 时长 =====
            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                Text {
                    text: "时长"
                    font.pixelSize: 14
                    font.bold: true
                    color: "#B0B0C0"
                    Layout.preferredWidth: 48
                }

                ComboBox {
                    id: durationCombo
                    Layout.fillWidth: true
                    model: ["30分钟", "1小时", "1.5小时", "2小时", "3小时"]
                    currentIndex: 1
                    font.pixelSize: 14

                    background: Rectangle {
                        implicitHeight: 40
                        radius: 6
                        color: "#1A1A2E"
                        border.color: durationCombo.activeFocus ? "#1E90FF" : "#404060"
                        border.width: 1
                    }

                    contentItem: Text {
                        leftPadding: 12
                        text: durationCombo.displayText
                        font.pixelSize: 14
                        color: "#FFFFFF"
                        verticalAlignment: Text.AlignVCenter
                    }

                    popup: Popup {
                        y: durationCombo.height
                        width: durationCombo.width
                        implicitHeight: contentItem.implicitHeight + 8
                        padding: 4

                        background: Rectangle {
                            color: "#2D2D4A"
                            radius: 8
                            border.color: "#3D3D5C"
                            border.width: 1
                        }

                        contentItem: ListView {
                            clip: true
                            implicitHeight: contentHeight
                            model: durationCombo.popup.visible ? durationCombo.delegateModel : null
                            currentIndex: durationCombo.highlightedIndex
                        }
                    }

                    delegate: ItemDelegate {
                        width: durationCombo.width
                        height: 36

                        background: Rectangle {
                            color: highlighted ? "#3D3D5C" : "transparent"
                            radius: 4
                        }

                        contentItem: Text {
                            text: modelData
                            font.pixelSize: 14
                            color: "#FFFFFF"
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 12
                        }

                        highlighted: durationCombo.highlightedIndex === index
                    }
                }
            }

            // ===== 会议设置 =====
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                CheckBox {
                    id: muteOnJoinCheck
                    text: "参会者入会时静音"
                    checked: true
                    font.pixelSize: 13

                    contentItem: Text {
                        text: parent.text
                        font.pixelSize: 13
                        color: "#D0D0E0"
                        leftPadding: parent.indicator.width + parent.spacing
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                CheckBox {
                    id: recordCheck
                    text: "自动录制会议"
                    font.pixelSize: 13

                    contentItem: Text {
                        text: parent.text
                        font.pixelSize: 13
                        color: "#D0D0E0"
                        leftPadding: parent.indicator.width + parent.spacing
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            // 状态文本
            Text {
                id: statusText
                Layout.fillWidth: true
                text: ""
                font.pixelSize: 13
                color: "#808090"
                horizontalAlignment: Text.AlignHCenter
                visible: text.length > 0
            }

            // 按钮
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    text: "取消"

                    background: Rectangle {
                        radius: 8
                        color: parent.pressed ? "#2D2D4C" :
                               parent.hovered ? "#4D4D6C" : "#3D3D5C"
                    }

                    contentItem: Text {
                        text: parent.text
                        font.pixelSize: 14
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: scheduleMeetingDialog.close()
                }

                Button {
                    id: submitBtn
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    text: "预定会议"

                    background: Rectangle {
                        radius: 8
                        color: parent.enabled ? (parent.pressed ? "#E65100" :
                               parent.hovered ? "#FF9800" : "#FF8F00") : "#555"

                        Behavior on color { ColorAnimation { duration: 150 } }
                    }

                    contentItem: Text {
                        text: parent.text
                        font.pixelSize: 14
                        font.bold: true
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: submitSchedule()
                }
            }
        }
    }

    onOpened: {
        titleField.text = (meetingController ? meetingController.userName : "我") + "的会议"
        titleField.selectAll()
        titleField.forceActiveFocus()
        statusText.text = ""
        calendarPopup.visible = false
        timePopup.visible = false
        successOverlay.visible = false

        // 重置为当前日期时间
        var now = new Date()
        selectedYear = now.getFullYear()
        selectedMonth = now.getMonth()
        selectedDay = now.getDate()
        viewYear = selectedYear
        viewMonth = selectedMonth

        // 默认取下一个整刻钟
        var h = now.getHours()
        var m = Math.ceil(now.getMinutes() / 15) * 15
        if (m >= 60) { h++; m = 0 }
        if (h >= 24) h = 23
        selectedTime = String(h).padStart(2, '0') + ":" + String(m % 60).padStart(2, '0')
    }
}
