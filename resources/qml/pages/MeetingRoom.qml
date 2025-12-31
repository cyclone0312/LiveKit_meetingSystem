import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15
import "../components"

Page {
    id: meetingRoom
    
    signal leaveMeeting()
    
    property bool showChat: false
    property bool showParticipants: false
    
    background: Rectangle {
        color: "#0D0D1A"
    }
    
    // 顶部信息栏
    TopBar {
        id: topBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
    }
    
    // 主内容区
    Item {
        anchors.left: parent.left
        anchors.right: rightPanel.left
        anchors.top: topBar.bottom
        anchors.bottom: controlBar.top
        
        // 视频网格
        VideoGrid {
            id: videoGrid
            anchors.fill: parent
            anchors.margins: 8
        }
    }
    
    // 右侧面板（参会者/聊天）
    Rectangle {
        id: rightPanel
        anchors.right: parent.right
        anchors.top: topBar.bottom
        anchors.bottom: controlBar.top
        width: (showChat || showParticipants) ? 320 : 0
        color: "#1A1A2E"
        clip: true
        
        Behavior on width {
            NumberAnimation { duration: 200; easing.type: Easing.OutQuad }
        }
        
        // 面板标签
        Rectangle {
            id: panelHeader
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 48
            color: "#252542"
            visible: parent.width > 0
            
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 8
                spacing: 8
                
                Text {
                    text: showChat ? "聊天" : "参会者"
                    font.pixelSize: 16
                    font.bold: true
                    color: "#FFFFFF"
                }
                
                Item { Layout.fillWidth: true }
                
                // 切换按钮
                Button {
                    implicitWidth: 32
                    implicitHeight: 32
                    flat: true
                    visible: showChat
                    
                    background: Rectangle {
                        radius: 6
                        color: parent.hovered ? "#3D3D5C" : "transparent"
                    }
                    
                    Text {
                        anchors.centerIn: parent
                        text: "👥"
                        font.pixelSize: 16
                    }
                    
                    onClicked: {
                        showChat = false
                        showParticipants = true
                    }
                }
                
                Button {
                    implicitWidth: 32
                    implicitHeight: 32
                    flat: true
                    visible: showParticipants
                    
                    background: Rectangle {
                        radius: 6
                        color: parent.hovered ? "#3D3D5C" : "transparent"
                    }
                    
                    Text {
                        anchors.centerIn: parent
                        text: "💬"
                        font.pixelSize: 16
                    }
                    
                    onClicked: {
                        showChat = true
                        showParticipants = false
                    }
                }
                
                // 关闭按钮
                Button {
                    implicitWidth: 32
                    implicitHeight: 32
                    flat: true
                    
                    background: Rectangle {
                        radius: 6
                        color: parent.hovered ? "#3D3D5C" : "transparent"
                    }
                    
                    Text {
                        anchors.centerIn: parent
                        text: "✕"
                        font.pixelSize: 14
                        color: "#B0B0C0"
                    }
                    
                    onClicked: {
                        showChat = false
                        showParticipants = false
                    }
                }
            }
        }
        
        // 参会者面板
        ParticipantPanel {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: panelHeader.bottom
            anchors.bottom: parent.bottom
            visible: showParticipants
        }
        
        // 聊天面板
        ChatPanel {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: panelHeader.bottom
            anchors.bottom: parent.bottom
            visible: showChat
        }
    }
    
    // 底部控制栏
    ControlBar {
        id: controlBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        
        onToggleMic: meetingController.toggleMic()
        onToggleCamera: meetingController.toggleCamera()
        onToggleScreenShare: meetingController.toggleScreenShare()
        onToggleRecord: meetingController.toggleRecording()
        onToggleHandRaise: meetingController.toggleHandRaise()
        
        onToggleParticipants: {
            if (showParticipants) {
                showParticipants = false
            } else {
                showParticipants = true
                showChat = false
                chatModel.markAllAsRead()
            }
        }
        
        onToggleChat: {
            if (showChat) {
                showChat = false
            } else {
                showChat = true
                showParticipants = false
                chatModel.markAllAsRead()
            }
        }
        
        onInviteClicked: meetingController.inviteParticipants()
        onSettingsClicked: settingsDialog.open()
        onLeaveClicked: leaveConfirmDialog.open()
    }
    
    // 离开会议确认对话框
    Dialog {
        id: leaveConfirmDialog
        anchors.centerIn: parent
        width: 360
        title: "离开会议"
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
                text: "离开会议"
                font.pixelSize: 18
                font.bold: true
                color: "#FFFFFF"
            }
        }
        
        contentItem: ColumnLayout {
            spacing: 24
            
            Text {
                Layout.fillWidth: true
                text: "确定要离开当前会议吗？"
                font.pixelSize: 14
                color: "#B0B0C0"
                horizontalAlignment: Text.AlignHCenter
            }
            
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
                    
                    onClicked: leaveConfirmDialog.close()
                }
                
                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    text: "离开会议"
                    
                    background: Rectangle {
                        radius: 8
                        color: parent.pressed ? "#C62828" : 
                               parent.hovered ? "#EF5350" : "#F44336"
                    }
                    
                    contentItem: Text {
                        text: parent.text
                        font.pixelSize: 14
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    
                    onClicked: {
                        leaveConfirmDialog.close()
                        leaveMeeting()
                    }
                }
            }
        }
    }
}
