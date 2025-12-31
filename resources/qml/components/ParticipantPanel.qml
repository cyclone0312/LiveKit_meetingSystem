import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: participantPanel
    
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        
        // 搜索框
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            Layout.margins: 12
            radius: 8
            color: "#1A1A2E"
            
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8
                
                Text {
                    text: "🔍"
                    font.pixelSize: 14
                    opacity: 0.6
                }
                
                TextField {
                    id: searchField
                    Layout.fillWidth: true
                    placeholderText: "搜索参会者"
                    font.pixelSize: 13
                    color: "#FFFFFF"
                    
                    background: Rectangle {
                        color: "transparent"
                    }
                }
            }
        }
        
        // 参会者统计
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: "transparent"
            
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                
                Text {
                    text: "会议中 (" + participantModel.count + ")"
                    font.pixelSize: 13
                    font.bold: true
                    color: "#B0B0C0"
                }
                
                Item { Layout.fillWidth: true }
                
                // 全部静音按钮
                Button {
                    implicitWidth: 72
                    implicitHeight: 28
                    text: "全部静音"
                    font.pixelSize: 11
                    
                    background: Rectangle {
                        radius: 4
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
            }
        }
        
        // 参会者列表
        ListView {
            id: participantList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 4
            
            model: participantModel
            
            delegate: Rectangle {
                width: participantList.width
                height: 56
                color: itemArea.containsMouse ? "#2D2D4A" : "transparent"
                
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 12
                    
                    // 头像
                    Rectangle {
                        width: 36
                        height: 36
                        radius: 18
                        color: model.isHost ? "#1E90FF" : "#4D4D6C"
                        
                        Text {
                            anchors.centerIn: parent
                            text: model.name.charAt(0)
                            font.pixelSize: 14
                            font.bold: true
                            color: "white"
                        }
                    }
                    
                    // 名称和状态
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        
                        RowLayout {
                            spacing: 6
                            
                            Text {
                                text: model.name
                                font.pixelSize: 14
                                color: "#FFFFFF"
                            }
                            
                            // 主持人标签
                            Rectangle {
                                visible: model.isHost
                                width: 44
                                height: 18
                                radius: 4
                                color: "#1E90FF20"
                                
                                Text {
                                    anchors.centerIn: parent
                                    text: "主持人"
                                    font.pixelSize: 10
                                    color: "#1E90FF"
                                }
                            }
                        }
                        
                        // 状态图标
                        RowLayout {
                            spacing: 8
                            
                            Text {
                                text: model.isMicOn ? "🎤" : "🔇"
                                font.pixelSize: 12
                                opacity: model.isMicOn ? 1.0 : 0.5
                            }
                            
                            Text {
                                text: model.isCameraOn ? "📹" : "📷"
                                font.pixelSize: 12
                                opacity: model.isCameraOn ? 1.0 : 0.5
                            }
                            
                            Text {
                                visible: model.isHandRaised
                                text: "✋"
                                font.pixelSize: 12
                            }
                            
                            Text {
                                visible: model.isScreenSharing
                                text: "🖥"
                                font.pixelSize: 12
                            }
                        }
                    }
                    
                    // 更多操作
                    Button {
                        implicitWidth: 28
                        implicitHeight: 28
                        flat: true
                        visible: itemArea.containsMouse
                        
                        background: Rectangle {
                            radius: 4
                            color: parent.hovered ? "#3D3D5C" : "transparent"
                        }
                        
                        Text {
                            anchors.centerIn: parent
                            text: "⋮"
                            font.pixelSize: 16
                            color: "#B0B0C0"
                        }
                        
                        onClicked: participantMenu.popup()
                        
                        Menu {
                            id: participantMenu
                            
                            background: Rectangle {
                                implicitWidth: 140
                                color: "#252542"
                                radius: 8
                                border.color: "#404060"
                            }
                            
                            MenuItem {
                                text: model.isMicOn ? "静音" : "请求开麦"
                            }
                            MenuItem {
                                text: "设为主持人"
                                enabled: !model.isHost
                            }
                            MenuItem {
                                text: "移除"
                                enabled: !model.isHost
                            }
                        }
                    }
                }
                
                MouseArea {
                    id: itemArea
                    anchors.fill: parent
                    hoverEnabled: true
                    propagateComposedEvents: true
                    onClicked: mouse.accepted = false
                }
            }
        }
        
        // 邀请按钮
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            color: "#252542"
            
            Button {
                anchors.centerIn: parent
                width: parent.width - 24
                height: 40
                text: "邀请参会者"
                font.pixelSize: 14
                
                background: Rectangle {
                    radius: 8
                    color: parent.pressed ? "#1873CC" : 
                           parent.hovered ? "#4DA6FF" : "#1E90FF"
                }
                
                contentItem: RowLayout {
                    spacing: 8
                    
                    Text {
                        text: "➕"
                        font.pixelSize: 16
                    }
                    
                    Text {
                        text: "邀请参会者"
                        font.pixelSize: 14
                        color: "white"
                    }
                }
                
                onClicked: meetingController.inviteParticipants()
            }
        }
    }
}
