import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: controlBar
    height: 80
    color: "#1A1A2E"
    
    signal toggleMic()
    signal toggleCamera()
    signal toggleScreenShare()
    signal toggleRecord()
    signal toggleHandRaise()
    signal toggleParticipants()
    signal toggleChat()
    signal inviteClicked()
    signal settingsClicked()
    signal leaveClicked()
    
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        spacing: 8
        
        // 左侧控制按钮
        RowLayout {
            spacing: 4
            
            // 麦克风按钮
            MeetingButton {
                iconText: meetingController.isMicOn ? "🎤" : "🔇"
                labelText: meetingController.isMicOn ? "静音" : "解除静音"
                isActive: meetingController.isMicOn
                isDanger: !meetingController.isMicOn
                onClicked: toggleMic()
            }
            
            // 摄像头按钮
            MeetingButton {
                iconText: meetingController.isCameraOn ? "📹" : "📷"
                labelText: meetingController.isCameraOn ? "关闭视频" : "开启视频"
                isActive: meetingController.isCameraOn
                isDanger: !meetingController.isCameraOn
                onClicked: toggleCamera()
            }
            
            // 屏幕共享按钮
            MeetingButton {
                iconText: "🖥"
                labelText: meetingController.isScreenSharing ? "停止共享" : "共享屏幕"
                isActive: meetingController.isScreenSharing
                onClicked: toggleScreenShare()
            }
            
            // 录制按钮
            MeetingButton {
                iconText: meetingController.isRecording ? "⏺" : "⏺"
                labelText: meetingController.isRecording ? "停止录制" : "录制"
                isActive: meetingController.isRecording
                activeColor: "#F44336"
                onClicked: toggleRecord()
            }
        }
        
        Item { Layout.fillWidth: true }
        
        // 中间控制按钮
        RowLayout {
            spacing: 4
            
            // 举手按钮
            MeetingButton {
                iconText: "✋"
                labelText: meetingController.isHandRaised ? "放下手" : "举手"
                isActive: meetingController.isHandRaised
                activeColor: "#FF9800"
                onClicked: toggleHandRaise()
            }
            
            // 表情反应按钮
            MeetingButton {
                iconText: "😀"
                labelText: "反应"
                onClicked: {
                    // 显示表情选择器
                }
            }
        }
        
        Item { Layout.fillWidth: true }
        
        // 右侧控制按钮
        RowLayout {
            spacing: 4
            
            // 参会者按钮
            MeetingButton {
                iconText: "👥"
                labelText: "参会者"
                badgeCount: participantModel.count
                onClicked: toggleParticipants()
            }
            
            // 聊天按钮
            MeetingButton {
                iconText: "💬"
                labelText: "聊天"
                badgeCount: chatModel.unreadCount
                onClicked: toggleChat()
            }
            
            // 邀请按钮
            MeetingButton {
                iconText: "➕"
                labelText: "邀请"
                onClicked: inviteClicked()
            }
            
            // 设置按钮
            MeetingButton {
                iconText: "⚙"
                labelText: "设置"
                onClicked: settingsClicked()
            }
            
            // 分隔线
            Rectangle {
                width: 1
                height: 40
                color: "#404060"
            }
            
            // 离开会议按钮
            Button {
                implicitWidth: 100
                implicitHeight: 44
                
                background: Rectangle {
                    radius: 8
                    color: parent.pressed ? "#C62828" : 
                           parent.hovered ? "#EF5350" : "#F44336"
                    
                    Behavior on color {
                        ColorAnimation { duration: 150 }
                    }
                }
                
                contentItem: Text {
                    text: "离开会议"
                    font.pixelSize: 14
                    font.bold: true
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                onClicked: leaveClicked()
            }
        }
    }
}
