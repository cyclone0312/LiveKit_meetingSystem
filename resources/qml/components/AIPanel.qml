import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

/**
 * AI 助手面板
 * 包含：AI 对话区 + 语音转录区 + 快捷功能按钮
 */
Item {
    id: aiPanel

    // 当前显示的子面板: "chat" | "transcript" | "minutes"
    property string currentTab: "chat"
    // AI 对话历史（本地维护）
    property var aiMessages: []
    // 会议纪要内容
    property string minutesContent: ""

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ====== 顶部标签切换 ======
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: "#1E1E3A"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 4

                // 对话标签
                TabBtn {
                    text: "🤖 对话"
                    isActive: currentTab === "chat"
                    onClicked: currentTab = "chat"
                }

                // 转录标签
                TabBtn {
                    text: "🎙 转录"
                    isActive: currentTab === "transcript"
                    onClicked: currentTab = "transcript"
                }

                // 纪要标签
                TabBtn {
                    text: "📋 纪要"
                    isActive: currentTab === "minutes"
                    onClicked: currentTab = "minutes"
                }
            }
        }

        // ====== 对话面板 ======
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: currentTab === "chat"

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // 快捷功能按钮行
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    color: "#252542"

                    ScrollView {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        ScrollBar.horizontal.policy: ScrollBar.AsNeeded
                        ScrollBar.vertical.policy: ScrollBar.AlwaysOff

                        Row {
                            spacing: 6
                            anchors.verticalCenter: parent.verticalCenter

                            QuickActionBtn {
                                text: "📝 总结讨论"
                                onClicked: sendAIMessage("请总结一下目前的讨论内容")
                            }
                            QuickActionBtn {
                                text: "📋 提取待办"
                                onClicked: sendAIMessage("请从讨论中提取所有待办事项和负责人")
                            }
                            QuickActionBtn {
                                text: "🌐 翻译"
                                onClicked: {
                                    aiInput.text = "请翻译：";
                                    aiInput.forceActiveFocus();
                                }
                            }
                            QuickActionBtn {
                                text: "💡 议程建议"
                                onClicked: sendAIMessage("请根据当前会议主题建议一个讨论议程")
                            }
                        }
                    }
                }

                // AI 对话消息列表
                ListView {
                    id: aiChatList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.margins: 8
                    clip: true
                    spacing: 10

                    model: ListModel { id: aiChatModel }

                    delegate: Item {
                        width: aiChatList.width
                        height: aiMsgCol.height + 8

                        Column {
                            id: aiMsgCol
                            width: parent.width
                            spacing: 4

                            // 发送者标识
                            Text {
                                text: model.isAI ? "🤖 小会" : ("👤 " + model.sender)
                                font.pixelSize: 11
                                font.bold: true
                                color: model.isAI ? "#4FC3F7" : "#B0B0C0"
                                anchors.left: model.isAI ? parent.left : undefined
                                anchors.right: model.isAI ? undefined : parent.right
                            }

                            // 消息气泡
                            Rectangle {
                                width: Math.min(aiMsgText.implicitWidth + 24, aiChatList.width * 0.85)
                                height: aiMsgText.height + 16
                                radius: 12
                                color: model.isAI ? "#1A3A5C" : "#1E90FF"
                                border.color: model.isAI ? "#2A5A8C" : "transparent"
                                border.width: model.isAI ? 1 : 0
                                anchors.left: model.isAI ? parent.left : undefined
                                anchors.right: model.isAI ? undefined : parent.right

                                Text {
                                    id: aiMsgText
                                    anchors.centerIn: parent
                                    width: Math.min(implicitWidth, aiChatList.width * 0.8)
                                    text: model.content
                                    font.pixelSize: 13
                                    color: "white"
                                    wrapMode: Text.Wrap
                                    textFormat: Text.PlainText
                                }
                            }
                        }
                    }

                    onCountChanged: {
                        Qt.callLater(function () {
                            aiChatList.positionViewAtEnd();
                        });
                    }

                    // 空状态
                    Item {
                        anchors.fill: parent
                        visible: aiChatModel.count === 0

                        Column {
                            anchors.centerIn: parent
                            spacing: 12

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "🤖"
                                font.pixelSize: 48
                                opacity: 0.3
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "AI 助手 · 小会"
                                font.pixelSize: 16
                                font.bold: true
                                color: "#808090"
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "输入问题或使用快捷功能"
                                font.pixelSize: 12
                                color: "#606070"
                            }
                        }
                    }
                }

                // AI 忙碌指示器
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: aiAssistant.isBusy ? 28 : 0
                    color: "#252542"
                    visible: aiAssistant.isBusy
                    clip: true

                    Behavior on Layout.preferredHeight {
                        NumberAnimation { duration: 150 }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        spacing: 8

                        BusyIndicator {
                            Layout.preferredWidth: 16
                            Layout.preferredHeight: 16
                            running: aiAssistant.isBusy
                        }

                        Text {
                            text: "小会正在思考..."
                            font.pixelSize: 12
                            color: "#4FC3F7"
                        }
                    }
                }

                // 输入区域
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 56
                    color: "#252542"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 6

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 38
                            radius: 8
                            color: "#1A1A2E"

                            TextField {
                                id: aiInput
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                placeholderText: "向 AI 助手提问..."
                                font.pixelSize: 13
                                color: "#FFFFFF"
                                enabled: !aiAssistant.isBusy

                                background: Rectangle { color: "transparent" }

                                Keys.onReturnPressed: sendCurrentMessage()
                                Keys.onEnterPressed: sendCurrentMessage()

                                function sendCurrentMessage() {
                                    if (text.trim().length > 0 && !aiAssistant.isBusy) {
                                        sendAIMessage(text.trim());
                                        text = "";
                                    }
                                }
                            }
                        }

                        Button {
                            implicitWidth: 38
                            implicitHeight: 38
                            enabled: aiInput.text.trim().length > 0 && !aiAssistant.isBusy

                            background: Rectangle {
                                radius: 8
                                color: parent.enabled ?
                                    (parent.pressed ? "#1873CC" :
                                     parent.hovered ? "#4DA6FF" : "#1E90FF") :
                                    "#3D3D5C"
                            }

                            Text {
                                anchors.centerIn: parent
                                text: "➤"
                                font.pixelSize: 16
                                color: parent.enabled ? "white" : "#606070"
                                rotation: -45
                            }

                            onClicked: aiInput.sendCurrentMessage()
                        }
                    }
                }
            }
        }

        // ====== 语音转录面板 ======
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: currentTab === "transcript"

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // 转录控制栏
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 52
                    color: "#252542"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 8

                        // 录音/转录状态指示
                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: aiAssistant.isRecordingAudio ? "#F44336" :
                                   aiAssistant.isTranscribing ? "#FF9800" : "#757575"

                            SequentialAnimation on opacity {
                                running: aiAssistant.isRecordingAudio || aiAssistant.isTranscribing
                                loops: Animation.Infinite
                                NumberAnimation { to: 0.3; duration: 600 }
                                NumberAnimation { to: 1.0; duration: 600 }
                            }
                        }

                        Column {
                            spacing: 2
                            Text {
                                text: aiAssistant.isRecordingAudio ? "● 录音中" :
                                      aiAssistant.isTranscribing ? "⏳ 识别中..." : "就绪"
                                font.pixelSize: 13
                                color: aiAssistant.isRecordingAudio ? "#F44336" :
                                       aiAssistant.isTranscribing ? "#FF9800" : "#808090"
                            }
                            Text {
                                visible: aiAssistant.isRecordingAudio
                                text: aiAssistant.recordingDuration
                                font.pixelSize: 11
                                color: "#B0B0C0"
                                font.family: "Consolas"
                            }
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            text: aiAssistant.transcriptCount + " 段"
                            font.pixelSize: 12
                            color: "#808090"
                        }

                        // 开始录音 按钮
                        Button {
                            implicitWidth: 90
                            implicitHeight: 32
                            visible: !aiAssistant.isRecordingAudio && !aiAssistant.isTranscribing
                            enabled: !aiAssistant.isTranscribing

                            background: Rectangle {
                                radius: 6
                                color: parent.hovered ? "#66BB6A" : "#4CAF50"
                            }

                            contentItem: Text {
                                text: "🎙 开始录音"
                                font.pixelSize: 12
                                color: "white"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            onClicked: {
                                // 自动开启麦克风
                                if (!meetingController.isMicOn) {
                                    meetingController.toggleMic();
                                }
                                aiAssistant.startRecording();
                            }
                        }

                        // 停止并转录 按钮
                        Button {
                            implicitWidth: 100
                            implicitHeight: 32
                            visible: aiAssistant.isRecordingAudio

                            background: Rectangle {
                                radius: 6
                                color: parent.hovered ? "#EF5350" : "#F44336"
                            }

                            contentItem: Text {
                                text: "⏹ 停止并转录"
                                font.pixelSize: 12
                                color: "white"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            onClicked: {
                                aiAssistant.stopRecordingAndTranscribe();
                            }
                        }

                        // 转录本地文件 按钮
                        Button {
                            implicitWidth: 100
                            implicitHeight: 32
                            visible: !aiAssistant.isRecordingAudio && !aiAssistant.isTranscribing
                            enabled: !aiAssistant.isTranscribing && aiAssistant.localRecordings.length > 0

                            background: Rectangle {
                                radius: 6
                                color: parent.enabled ? (parent.hovered ? "#42A5F5" : "#1E90FF") : "#555"
                            }

                            contentItem: Text {
                                text: "📂 转录文件"
                                font.pixelSize: 12
                                color: "white"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            onClicked: {
                                localFilePopup.open()
                            }
                        }
                    }
                }

                // 转录进度提示（等待服务端返回时显示）
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: aiAssistant.isTranscribing ? 40 : 0
                    color: "#1E2A2E"
                    visible: aiAssistant.isTranscribing
                    clip: true

                    Behavior on Layout.preferredHeight {
                        NumberAnimation { duration: 200 }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 8

                        BusyIndicator {
                            Layout.preferredWidth: 20
                            Layout.preferredHeight: 20
                            running: aiAssistant.isTranscribing
                        }

                        Text {
                            text: "正在将录音转为文字，这可能需要 10~60 秒..."
                            font.pixelSize: 12
                            color: "#FF9800"
                        }
                    }

                    SequentialAnimation on opacity {
                        running: parent.visible
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.6; duration: 1200 }
                        NumberAnimation { to: 1.0; duration: 1200 }
                    }
                }

                // 转录内容列表
                ListView {
                    id: transcriptList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.margins: 8
                    clip: true
                    spacing: 8

                    model: ListModel { id: transcriptModel }

                    delegate: Rectangle {
                        width: transcriptList.width
                        height: transcriptCol.height + 12
                        radius: 8
                        color: "#1A1A2E"

                        Column {
                            id: transcriptCol
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 8
                            spacing: 4

                            RowLayout {
                                width: parent.width
                                Text {
                                    text: "🎙 " + model.speaker
                                    font.pixelSize: 11
                                    font.bold: true
                                    color: "#4FC3F7"
                                }
                                Item { Layout.fillWidth: true }
                                Text {
                                    text: model.time
                                    font.pixelSize: 10
                                    color: "#606070"
                                }
                            }

                            Text {
                                width: parent.width
                                text: model.text
                                font.pixelSize: 13
                                color: "#E0E0E0"
                                wrapMode: Text.Wrap
                            }
                        }
                    }

                    onCountChanged: {
                        Qt.callLater(function () {
                            transcriptList.positionViewAtEnd();
                        });
                    }

                    // 空状态
                    Item {
                        anchors.fill: parent
                        visible: transcriptModel.count === 0

                        Column {
                            anchors.centerIn: parent
                            spacing: 12

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "🎙"
                                font.pixelSize: 48
                                opacity: 0.3
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "语音转录"
                                font.pixelSize: 16
                                font.bold: true
                                color: "#808090"
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "点击\"开始录音\"录制后自动转为文字"
                                font.pixelSize: 12
                                color: "#606070"
                            }
                        }
                    }
                }
            }
        }

        // ====== 会议纪要面板 ======
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: currentTab === "minutes"

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // 操作栏
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    color: "#252542"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 8

                        Button {
                            implicitWidth: 120
                            implicitHeight: 32
                            enabled: !aiAssistant.isBusy

                            background: Rectangle {
                                radius: 6
                                color: parent.enabled ?
                                    (parent.hovered ? "#4DA6FF" : "#1E90FF") :
                                    "#3D3D5C"
                            }

                            contentItem: Text {
                                text: aiAssistant.isBusy ? "生成中..." : "生成会议纪要"
                                font.pixelSize: 12
                                color: parent.enabled ? "white" : "#808090"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            onClicked: {
                                // 收集聊天记录传给 AI
                                aiAssistant.generateMeetingMinutes([]);
                            }
                        }

                        Item { Layout.fillWidth: true }

                        // 复制按钮
                        Button {
                            implicitWidth: 60
                            implicitHeight: 32
                            visible: minutesContent.length > 0

                            background: Rectangle {
                                radius: 6
                                color: parent.hovered ? "#3D3D5C" : "transparent"
                                border.color: "#3D3D5C"
                                border.width: 1
                            }

                            contentItem: Text {
                                text: "📋 复制"
                                font.pixelSize: 12
                                color: "#B0B0C0"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            onClicked: {
                                // 复制到剪贴板
                                meetingController.copyMeetingInfo();
                            }
                        }
                    }
                }

                // 纪要显示
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.margins: 12

                    TextArea {
                        id: minutesTextArea
                        text: minutesContent.length > 0 ? minutesContent : "点击\"生成会议纪要\"按钮，AI 将根据语音转录和聊天记录自动生成。"
                        font.pixelSize: 13
                        color: minutesContent.length > 0 ? "#E0E0E0" : "#606070"
                        wrapMode: Text.Wrap
                        readOnly: true

                        background: Rectangle {
                            color: "#1A1A2E"
                            radius: 8
                        }
                    }
                }

                // 忙碌指示
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: aiAssistant.isBusy ? 32 : 0
                    color: "#252542"
                    visible: aiAssistant.isBusy
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
                            text: "正在生成会议纪要..."
                            font.pixelSize: 12
                            color: "#4FC3F7"
                        }
                    }
                }
            }
        }
    }

    // ====== 信号连接 ======

    Connections {
        target: aiAssistant

        function onAiReplyReceived(reply) {
            aiChatModel.append({
                "sender": "小会",
                "content": reply,
                "isAI": true
            });
        }

        function onAiError(error) {
            aiChatModel.append({
                "sender": "系统",
                "content": "❌ " + error,
                "isAI": true
            });
        }

        function onNewTranscriptReceived(text) {
            transcriptModel.append({
                "speaker": aiAssistant.userName || "发言者",
                "time": Qt.formatTime(new Date(), "HH:mm:ss"),
                "text": text
            });
        }

        function onMeetingMinutesReceived(minutes) {
            minutesContent = minutes;
        }

        function onSummaryReceived(summary) {
            aiChatModel.append({
                "sender": "小会",
                "content": summary,
                "isAI": true
            });
        }

        function onTranscriptionCompleted(fullText) {
            console.log("[AIPanel] 转录完成: " + fullText.substring(0, 100));
        }

        function onTranscriptionFailed(error) {
            // 在转录面板显示错误提示
            transcriptModel.append({
                "speaker": "系统",
                "time": Qt.formatTime(new Date(), "HH:mm:ss"),
                "text": "❌ 转录失败: " + error
            });
        }

        function onRecordingSaved(filePath) {
            console.log("[AIPanel] 录音已保存: " + filePath);
        }

        function onFileTranscriptionCompleted(filePath, transcript) {
            console.log("[AIPanel] 文件转录完成: " + filePath);
            localFilePopup.close();
        }
    }

    // ====== 辅助函数 ======

    function sendAIMessage(message) {
        // 添加用户消息到聊天列表
        aiChatModel.append({
            "sender": meetingController.userName || "我",
            "content": message,
            "isAI": false
        });
        // 调用 AI 后端
        aiAssistant.sendMessage(message);
    }

    // ====== 自定义组件 ======

    // 标签按钮
    component TabBtn: Button {
        property bool isActive: false

        implicitWidth: Math.max(70, contentItem.implicitWidth + 16)
        implicitHeight: 32
        flat: true

        background: Rectangle {
            radius: 6
            color: isActive ? "#1E90FF" :
                   (parent.hovered ? "#3D3D5C" : "transparent")
        }

        contentItem: Text {
            text: parent.text
            font.pixelSize: 12
            font.bold: isActive
            color: isActive ? "white" : "#B0B0C0"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    // 快捷功能按钮
    component QuickActionBtn: Button {
        implicitHeight: 28
        implicitWidth: Math.max(80, contentItem.implicitWidth + 16)
        flat: true

        background: Rectangle {
            radius: 14
            color: parent.pressed ? "#2A4A6C" :
                   parent.hovered ? "#1A3A5C" : "#252542"
            border.color: "#3A5A7C"
            border.width: 1
        }

        contentItem: Text {
            text: parent.text
            font.pixelSize: 11
            color: "#4FC3F7"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    // ====== 本地录音选择弹窗 ======
    Popup {
        id: localFilePopup
        anchors.centerIn: parent
        width: 280
        height: Math.min(350, 60 + aiAssistant.localRecordings.length * 60)
        modal: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: "#252542"
            radius: 12
            border.color: "#3D3D5C"
            border.width: 1
        }

        contentItem: ColumnLayout {
            spacing: 8

            Text {
                text: "选择录音文件"
                font.pixelSize: 14
                font.bold: true
                color: "#FFFFFF"
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }

            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 4
                model: aiAssistant.localRecordings

                delegate: Rectangle {
                    width: ListView.view.width
                    height: 52
                    radius: 6
                    color: fileItemArea.containsMouse ? "#3D3D5C" : "#2D2D4A"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 8

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Text {
                                text: modelData.meetingTitle || modelData.fileName
                                font.pixelSize: 12
                                color: "#FFFFFF"
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Text {
                                text: modelData.durationStr + " | " + modelData.fileSizeStr + " | " + modelData.dateTime
                                font.pixelSize: 10
                                color: "#808090"
                            }
                        }

                        Button {
                            implicitWidth: 48
                            implicitHeight: 24
                            background: Rectangle {
                                radius: 4
                                color: parent.hovered ? "#66BB6A" : "#4CAF50"
                            }
                            contentItem: Text {
                                text: "转录"
                                font.pixelSize: 10
                                color: "white"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: {
                                aiAssistant.transcribeLocalFile(modelData.filePath);
                            }
                        }
                    }

                    MouseArea {
                        id: fileItemArea
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.NoButton
                    }
                }
            }
        }
    }
}
