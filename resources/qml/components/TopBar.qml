import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: topBar
    height: 48
    color: "#1A1A2E"
    
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 16
        
        // 会议信息
        RowLayout {
            spacing: 8
            
            // 安全图标
            Rectangle {
                width: 24
                height: 24
                radius: 4
                color: "#4CAF50"
                
                Text {
                    anchors.centerIn: parent
                    text: "🔒"
                    font.pixelSize: 12
                }
            }
            
            // 会议标题
            Text {
                text: meetingController.meetingTitle
                font.pixelSize: 14
                font.bold: true
                color: "#FFFFFF"
            }
            
            // 分隔符
            Text {
                text: "|"
                font.pixelSize: 14
                color: "#606070"
            }
            
            // 会议号
            Text {
                text: "会议号: " + meetingController.meetingId
                font.pixelSize: 13
                color: "#B0B0C0"
                
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: meetingController.copyMeetingInfo()
                    
                    ToolTip.visible: containsMouse
                    ToolTip.text: "点击复制会议号"
                    ToolTip.delay: 500
                }
            }
        }
        
        Item { Layout.fillWidth: true }
        
        // 中间 - 会议时长
        Rectangle {
            Layout.preferredWidth: 80
            Layout.preferredHeight: 28
            radius: 14
            color: "#252542"
            
            RowLayout {
                anchors.centerIn: parent
                spacing: 6
                
                // 录制指示器（如果正在录制）
                Rectangle {
                    visible: meetingController.isRecording
                    width: 8
                    height: 8
                    radius: 4
                    color: "#F44336"
                    
                    SequentialAnimation on opacity {
                        running: meetingController.isRecording
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.3; duration: 500 }
                        NumberAnimation { to: 1; duration: 500 }
                    }
                }
                
                Text {
                    text: meetingController.meetingDuration
                    font.pixelSize: 13
                    font.family: "Consolas"
                    color: "#FFFFFF"
                }
            }
        }
        
        Item { Layout.fillWidth: true }
        
        // 右侧按钮
        RowLayout {
            spacing: 8
            
            // 网络状态
            Rectangle {
                width: 32
                height: 32
                radius: 6
                color: networkArea.containsMouse ? "#3D3D5C" : "transparent"
                
                Text {
                    anchors.centerIn: parent
                    text: "📶"
                    font.pixelSize: 16
                }
                
                MouseArea {
                    id: networkArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    
                    ToolTip.visible: containsMouse
                    ToolTip.text: "网络状态: 良好"
                    ToolTip.delay: 500
                }
            }
            
            // 全屏按钮
            Rectangle {
                width: 32
                height: 32
                radius: 6
                color: fullscreenArea.containsMouse ? "#3D3D5C" : "transparent"
                
                Text {
                    anchors.centerIn: parent
                    text: "⛶"
                    font.pixelSize: 16
                    color: "#B0B0C0"
                }
                
                MouseArea {
                    id: fullscreenArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (mainWindow.visibility === Window.FullScreen) {
                            mainWindow.showNormal()
                        } else {
                            mainWindow.showFullScreen()
                        }
                    }
                }
            }
            
            // 视图切换
            Rectangle {
                width: 32
                height: 32
                radius: 6
                color: viewArea.containsMouse ? "#3D3D5C" : "transparent"
                
                Text {
                    anchors.centerIn: parent
                    text: "⊞"
                    font.pixelSize: 16
                    color: "#B0B0C0"
                }
                
                MouseArea {
                    id: viewArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: viewMenu.open()
                }
                
                Menu {
                    id: viewMenu
                    y: parent.height + 4
                    
                    background: Rectangle {
                        implicitWidth: 160
                        color: "#252542"
                        radius: 8
                        border.color: "#404060"
                    }
                    
                    MenuItem {
                        text: "宫格视图"
                        onTriggered: meetingController.switchView("grid")
                    }
                    MenuItem {
                        text: "演讲者视图"
                        onTriggered: meetingController.switchView("speaker")
                    }
                    MenuItem {
                        text: "画廊视图"
                        onTriggered: meetingController.switchView("gallery")
                    }
                }
            }
        }
    }
}
