import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: chatPanel
    
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        
        // 聊天消息列表
        ListView {
            id: chatList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 12
            clip: true
            spacing: 12
            
            model: chatModel
            
            delegate: Item {
                width: chatList.width
                height: messageContent.height + 8
                
                // 系统消息
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: model.isSystem
                    width: systemText.width + 24
                    height: systemText.height + 8
                    radius: 12
                    color: "#3D3D5C40"
                    
                    Text {
                        id: systemText
                        anchors.centerIn: parent
                        text: model.content
                        font.pixelSize: 12
                        color: "#808090"
                    }
                }
                
                // 普通消息
                RowLayout {
                    anchors.fill: parent
                    visible: !model.isSystem
                    layoutDirection: model.isSelf ? Qt.RightToLeft : Qt.LeftToRight
                    spacing: 8
                    
                    // 头像（非自己的消息显示）
                    Rectangle {
                        visible: !model.isSelf
                        width: 32
                        height: 32
                        radius: 16
                        color: "#1E90FF"
                        
                        Text {
                            anchors.centerIn: parent
                            text: model.senderName.charAt(0)
                            font.pixelSize: 12
                            font.bold: true
                            color: "white"
                        }
                    }
                    
                    // 消息内容
                    Column {
                        id: messageContent
                        spacing: 4
                        Layout.maximumWidth: chatList.width * 0.7
                        
                        // 发送者名称（非自己的消息显示）
                        Text {
                            visible: !model.isSelf
                            text: model.senderName
                            font.pixelSize: 11
                            color: "#808090"
                        }
                        
                        // 消息气泡
                        Rectangle {
                            width: messageText.width + 24
                            height: messageText.height + 16
                            radius: 12
                            color: model.isSelf ? "#1E90FF" : "#3D3D5C"
                            
                            Text {
                                id: messageText
                                anchors.centerIn: parent
                                text: model.content
                                font.pixelSize: 13
                                color: "white"
                                wrapMode: Text.Wrap
                                width: Math.min(implicitWidth, chatList.width * 0.6)
                            }
                        }
                        
                        // 时间戳
                        Text {
                            anchors.right: model.isSelf ? parent.right : undefined
                            text: model.timeString
                            font.pixelSize: 10
                            color: "#606070"
                        }
                    }
                    
                    Item { Layout.fillWidth: true }
                }
            }
            
            // 自动滚动到底部
            onCountChanged: {
                Qt.callLater(function() {
                    chatList.positionViewAtEnd()
                })
            }
            
            // 空状态
            Item {
                anchors.fill: parent
                visible: chatModel.count === 0
                
                Column {
                    anchors.centerIn: parent
                    spacing: 12
                    
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "💬"
                        font.pixelSize: 48
                        opacity: 0.3
                    }
                    
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "暂无消息"
                        font.pixelSize: 14
                        color: "#606070"
                    }
                    
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "发送消息与参会者聊天"
                        font.pixelSize: 12
                        color: "#505060"
                    }
                }
            }
        }
        
        // 输入区域
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            color: "#252542"
            
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8
                
                // 表情按钮
                Button {
                    implicitWidth: 36
                    implicitHeight: 36
                    flat: true
                    
                    background: Rectangle {
                        radius: 6
                        color: parent.hovered ? "#3D3D5C" : "transparent"
                    }
                    
                    Text {
                        anchors.centerIn: parent
                        text: "😊"
                        font.pixelSize: 18
                    }
                }
                
                // 输入框
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    radius: 8
                    color: "#1A1A2E"
                    
                    TextField {
                        id: messageInput
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        placeholderText: "输入消息..."
                        font.pixelSize: 13
                        color: "#FFFFFF"
                        
                        background: Rectangle {
                            color: "transparent"
                        }
                        
                        Keys.onReturnPressed: sendMessage()
                        Keys.onEnterPressed: sendMessage()
                        
                        function sendMessage() {
                            if (text.trim().length > 0) {
                                chatModel.sendMessage(text.trim(), meetingController.userName)
                                text = ""
                            }
                        }
                    }
                }
                
                // 发送按钮
                Button {
                    implicitWidth: 40
                    implicitHeight: 40
                    enabled: messageInput.text.trim().length > 0
                    
                    background: Rectangle {
                        radius: 8
                        color: parent.enabled ? 
                               (parent.pressed ? "#1873CC" : 
                                parent.hovered ? "#4DA6FF" : "#1E90FF") : 
                               "#3D3D5C"
                        
                        Behavior on color {
                            ColorAnimation { duration: 150 }
                        }
                    }
                    
                    Text {
                        anchors.centerIn: parent
                        text: "➤"
                        font.pixelSize: 18
                        color: parent.enabled ? "white" : "#606070"
                        rotation: -45
                    }
                    
                    onClicked: messageInput.sendMessage()
                }
            }
        }
    }
    
    // 模拟消息按钮（仅用于测试）
    Button {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 8
        width: 24
        height: 24
        visible: false // 设为true可以测试收到消息
        
        background: Rectangle {
            radius: 4
            color: "#3D3D5C"
        }
        
        Text {
            anchors.centerIn: parent
            text: "+"
            font.pixelSize: 14
            color: "white"
        }
        
        onClicked: chatModel.simulateIncomingMessage()
    }
}
