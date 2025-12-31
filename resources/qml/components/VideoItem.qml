import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: videoItem
    
    property string participantName: ""
    property bool isMicOn: false
    property bool isCameraOn: false
    property bool isHost: false
    property bool isHandRaised: false
    property bool isScreenSharing: false
    
    radius: 8
    color: "#252542"
    border.color: isScreenSharing ? "#1E90FF" : "transparent"
    border.width: isScreenSharing ? 2 : 0
    
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
        
        // 当摄像头开启时显示模拟视频画面
        Rectangle {
            anchors.fill: parent
            radius: 6
            visible: isCameraOn
            
            // 模拟视频背景（实际应用中这里会是真实的视频流）
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#2D3748" }
                GradientStop { position: 1.0; color: "#1A202C" }
            }
            
            // 模拟摄像头图标
            Text {
                anchors.centerIn: parent
                text: "📷"
                font.pixelSize: 48
                opacity: 0.3
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
