import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtMultimedia

Dialog {
    id: settingsDialog
    
    anchors.centerIn: parent
    width: 560
    height: 480
    title: "设置"
    modal: true
    standardButtons: Dialog.NoButton
    
    background: Rectangle {
        color: "#252542"
        radius: 12
    }
    
    header: Rectangle {
        width: parent.width
        height: 56
        color: "transparent"
        
        Text {
            anchors.centerIn: parent
            text: "设置"
            font.pixelSize: 18
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
            
            onClicked: settingsDialog.close()
        }
    }
    
    contentItem: RowLayout {
        spacing: 0
        
        // 左侧菜单
        Rectangle {
            Layout.preferredWidth: 160
            Layout.fillHeight: true
            color: "#1A1A2E"
            radius: 8
            
            ListView {
                id: settingsMenu
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4
                currentIndex: 0
                
                model: ListModel {
                    ListElement { name: "音频"; icon: "🔊" }
                    ListElement { name: "视频"; icon: "📹" }
                    ListElement { name: "常规"; icon: "⚙" }
                    ListElement { name: "快捷键"; icon: "⌨" }
                    ListElement { name: "关于"; icon: "ℹ" }
                }
                
                delegate: Rectangle {
                    width: settingsMenu.width
                    height: 40
                    radius: 6
                    color: settingsMenu.currentIndex === index ? "#1E90FF" : 
                           menuItemArea.containsMouse ? "#3D3D5C" : "transparent"
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        spacing: 10
                        
                        Text {
                            text: model.icon
                            font.pixelSize: 14
                        }
                        
                        Text {
                            text: model.name
                            font.pixelSize: 14
                            color: settingsMenu.currentIndex === index ? "white" : "#B0B0C0"
                        }
                    }
                    
                    MouseArea {
                        id: menuItemArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: settingsMenu.currentIndex = index
                    }
                }
            }
        }
        
        // 右侧内容
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "transparent"
            
            StackLayout {
                anchors.fill: parent
                anchors.margins: 16
                currentIndex: settingsMenu.currentIndex
                
                // 音频设置
                ColumnLayout {
                    spacing: 16
                    
                    Text {
                        text: "音频设置"
                        font.pixelSize: 16
                        font.bold: true
                        color: "#FFFFFF"
                    }
                    
                    // 麦克风选择
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        
                        Text {
                            text: "麦克风"
                            font.pixelSize: 14
                            color: "#B0B0C0"
                        }
                        
                        ComboBox {
                            Layout.fillWidth: true
                            model: ["默认麦克风", "系统麦克风"]
                            font.pixelSize: 13
                            
                            background: Rectangle {
                                implicitHeight: 40
                                radius: 6
                                color: "#1A1A2E"
                                border.color: "#404060"
                            }
                        }
                        
                        // 麦克风音量
                        RowLayout {
                            spacing: 12
                            
                            Text {
                                text: "音量"
                                font.pixelSize: 13
                                color: "#808090"
                            }
                            
                            Slider {
                                Layout.fillWidth: true
                                from: 0
                                to: 100
                                value: 80
                            }
                        }
                    }
                    
                    // 扬声器选择
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        
                        Text {
                            text: "扬声器"
                            font.pixelSize: 14
                            color: "#B0B0C0"
                        }
                        
                        ComboBox {
                            Layout.fillWidth: true
                            model: ["默认扬声器", "系统扬声器"]
                            font.pixelSize: 13
                            
                            background: Rectangle {
                                implicitHeight: 40
                                radius: 6
                                color: "#1A1A2E"
                                border.color: "#404060"
                            }
                        }
                        
                        // 扬声器音量
                        RowLayout {
                            spacing: 12
                            
                            Text {
                                text: "音量"
                                font.pixelSize: 13
                                color: "#808090"
                            }
                            
                            Slider {
                                Layout.fillWidth: true
                                from: 0
                                to: 100
                                value: 100
                            }
                        }
                    }
                    
                    Item { Layout.fillHeight: true }
                }
                
                // 视频设置
                ColumnLayout {
                    id: videoSettingsLayout
                    spacing: 16
                    
                    // 内部属性：跟踪预览摄像头是否由设置页启动
                    property bool previewStartedBySettings: false
                    
                    Text {
                        text: "视频设置"
                        font.pixelSize: 16
                        font.bold: true
                        color: "#FFFFFF"
                    }
                    
                    // 摄像头选择
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        
                        Text {
                            text: "摄像头"
                            font.pixelSize: 14
                            color: "#B0B0C0"
                        }
                        
                        ComboBox {
                            id: cameraComboBox
                            Layout.fillWidth: true
                            model: mediaCapture ? mediaCapture.availableCameras : ["无可用摄像头"]
                            currentIndex: mediaCapture ? mediaCapture.currentCameraIndex : 0
                            font.pixelSize: 13
                            
                            background: Rectangle {
                                implicitHeight: 40
                                radius: 6
                                color: "#1A1A2E"
                                border.color: "#404060"
                            }
                            
                            contentItem: Text {
                                leftPadding: 12
                                text: cameraComboBox.displayText
                                font: cameraComboBox.font
                                color: "#FFFFFF"
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }
                            
                            popup: Popup {
                                y: cameraComboBox.height
                                width: cameraComboBox.width
                                implicitHeight: contentItem.implicitHeight + 2
                                padding: 1
                                
                                contentItem: ListView {
                                    clip: true
                                    implicitHeight: contentHeight
                                    model: cameraComboBox.popup.visible ? cameraComboBox.delegateModel : null
                                    ScrollIndicator.vertical: ScrollIndicator {}
                                }
                                
                                background: Rectangle {
                                    radius: 6
                                    color: "#1A1A2E"
                                    border.color: "#404060"
                                }
                            }
                            
                            delegate: ItemDelegate {
                                width: cameraComboBox.width
                                height: 36
                                contentItem: Text {
                                    text: modelData
                                    font.pixelSize: 13
                                    color: cameraComboBox.highlightedIndex === index ? "#FFFFFF" : "#B0B0C0"
                                    verticalAlignment: Text.AlignVCenter
                                    leftPadding: 8
                                }
                                background: Rectangle {
                                    color: cameraComboBox.highlightedIndex === index ? "#1E90FF" : "transparent"
                                }
                            }
                            
                            onActivated: function(index) {
                                if (mediaCapture) {
                                    mediaCapture.currentCameraIndex = index
                                }
                            }
                        }
                    }
                    
                    // 视频预览
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 180
                        radius: 8
                        color: "#1A1A2E"
                        clip: true
                        
                        // 摄像头预览画面
                        VideoOutput {
                            id: settingsVideoOutput
                            anchors.fill: parent
                            visible: mediaCapture !== null && mediaCapture.cameraActive
                            fillMode: VideoOutput.PreserveAspectCrop
                            
                            // 镜像显示（更自然）
                            transform: Scale {
                                origin.x: settingsVideoOutput.width / 2
                                xScale: -1
                            }
                        }
                        
                        // 占位文字（无摄像头或未启动时显示）
                        Text {
                            anchors.centerIn: parent
                            text: {
                                if (!mediaCapture || mediaCapture.availableCameras.length === 0)
                                    return "未检测到摄像头"
                                if (meetingController && meetingController.isInMeeting)
                                    return "会议中，预览不可用"
                                if (!mediaCapture.cameraActive)
                                    return "摄像头预览"
                                return ""
                            }
                            font.pixelSize: 14
                            color: "#606070"
                            visible: !settingsVideoOutput.visible
                        }
                    }
                    
                    Item { Layout.fillHeight: true }
                    
                    // 当视频设置页可见时启动摄像头预览
                    // settingsMenu.currentIndex === 1 表示当前选中"视频"标签
                    Connections {
                        target: settingsMenu
                        function onCurrentIndexChanged() {
                            if (settingsMenu.currentIndex === 1 && settingsDialog.visible) {
                                videoSettingsLayout.startPreviewIfNeeded()
                            } else {
                                videoSettingsLayout.stopPreviewIfNeeded()
                            }
                        }
                    }
                    
                    // 对话框可见性变化时管理摄像头
                    Connections {
                        target: settingsDialog
                        function onVisibleChanged() {
                            if (settingsDialog.visible && settingsMenu.currentIndex === 1) {
                                videoSettingsLayout.startPreviewIfNeeded()
                            } else {
                                videoSettingsLayout.stopPreviewIfNeeded()
                            }
                        }
                    }
                    
                    function startPreviewIfNeeded() {
                        if (!mediaCapture) return
                        // 会议中不抢占摄像头
                        if (meetingController && meetingController.isInMeeting) return
                        if (mediaCapture.availableCameras.length === 0) return
                        
                        if (!mediaCapture.cameraActive) {
                            mediaCapture.startCamera()
                            previewStartedBySettings = true
                        }
                        // 绑定 VideoOutput 的 sink 到 mediaCapture
                        if (settingsVideoOutput.videoSink) {
                            mediaCapture.bindVideoSink(settingsVideoOutput.videoSink)
                        }
                    }
                    
                    function stopPreviewIfNeeded() {
                        if (!mediaCapture) return
                        // 只有设置页自己启动的摄像头才由设置页停止
                        if (previewStartedBySettings && mediaCapture.cameraActive) {
                            mediaCapture.stopCamera()
                            previewStartedBySettings = false
                        }
                    }
                }
                
                // 常规设置
                ColumnLayout {
                    spacing: 16
                    
                    Text {
                        text: "常规设置"
                        font.pixelSize: 16
                        font.bold: true
                        color: "#FFFFFF"
                    }
                    
                    CheckBox {
                        text: "开机自动启动"
                        font.pixelSize: 13
                    }
                    
                    CheckBox {
                        text: "启动时最小化"
                        font.pixelSize: 13
                    }
                    
                    CheckBox {
                        text: "显示会议时长"
                        font.pixelSize: 13
                        checked: true
                    }
                    
                    CheckBox {
                        text: "入会时自动静音"
                        font.pixelSize: 13
                        checked: true
                    }
                    
                    CheckBox {
                        text: "入会时关闭摄像头"
                        font.pixelSize: 13
                    }
                    
                    Item { Layout.fillHeight: true }
                }
                
                // 快捷键设置
                ColumnLayout {
                    spacing: 16
                    
                    Text {
                        text: "快捷键"
                        font.pixelSize: 16
                        font.bold: true
                        color: "#FFFFFF"
                    }
                    
                    GridLayout {
                        columns: 2
                        rowSpacing: 12
                        columnSpacing: 16
                        
                        Text { text: "静音/取消静音"; color: "#B0B0C0"; font.pixelSize: 13 }
                        Rectangle {
                            width: 100
                            height: 28
                            radius: 4
                            color: "#1A1A2E"
                            Text {
                                anchors.centerIn: parent
                                text: "Alt + A"
                                font.pixelSize: 12
                                color: "#FFFFFF"
                            }
                        }
                        
                        Text { text: "开启/关闭视频"; color: "#B0B0C0"; font.pixelSize: 13 }
                        Rectangle {
                            width: 100
                            height: 28
                            radius: 4
                            color: "#1A1A2E"
                            Text {
                                anchors.centerIn: parent
                                text: "Alt + V"
                                font.pixelSize: 12
                                color: "#FFFFFF"
                            }
                        }
                        
                        Text { text: "屏幕共享"; color: "#B0B0C0"; font.pixelSize: 13 }
                        Rectangle {
                            width: 100
                            height: 28
                            radius: 4
                            color: "#1A1A2E"
                            Text {
                                anchors.centerIn: parent
                                text: "Alt + S"
                                font.pixelSize: 12
                                color: "#FFFFFF"
                            }
                        }
                        
                        Text { text: "举手"; color: "#B0B0C0"; font.pixelSize: 13 }
                        Rectangle {
                            width: 100
                            height: 28
                            radius: 4
                            color: "#1A1A2E"
                            Text {
                                anchors.centerIn: parent
                                text: "Alt + Y"
                                font.pixelSize: 12
                                color: "#FFFFFF"
                            }
                        }
                    }
                    
                    Item { Layout.fillHeight: true }
                }
                
                // 关于
                ColumnLayout {
                    spacing: 16
                    
                    Text {
                        text: "关于"
                        font.pixelSize: 16
                        font.bold: true
                        color: "#FFFFFF"
                    }
                    
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 120
                        radius: 8
                        color: "#1A1A2E"
                        
                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 8
                            
                            Rectangle {
                                Layout.alignment: Qt.AlignHCenter
                                width: 48
                                height: 48
                                radius: 10
                                color: "#1E90FF"
                                
                                Text {
                                    anchors.centerIn: parent
                                    text: "会"
                                    font.pixelSize: 24
                                    font.bold: true
                                    color: "white"
                                }
                            }
                            
                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: "视频会议"
                                font.pixelSize: 16
                                font.bold: true
                                color: "#FFFFFF"
                            }
                            
                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: "版本 1.0.0"
                                font.pixelSize: 13
                                color: "#808090"
                            }
                        }
                    }
                    
                    Text {
                        text: "使用 Qt Quick 构建的视频会议应用程序UI演示"
                        font.pixelSize: 13
                        color: "#808090"
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                    
                    Button {
                        text: "检查更新"
                        font.pixelSize: 13
                        
                        background: Rectangle {
                            implicitWidth: 100
                            implicitHeight: 36
                            radius: 6
                            color: parent.pressed ? "#2D2D4C" : 
                                   parent.hovered ? "#4D4D6C" : "#3D3D5C"
                        }
                        
                        contentItem: Text {
                            text: parent.text
                            font: parent.font
                            color: "white"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                    
                    Item { Layout.fillHeight: true }
                }
            }
        }
    }
}
