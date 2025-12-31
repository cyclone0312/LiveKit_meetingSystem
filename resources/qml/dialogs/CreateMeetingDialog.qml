import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Dialog {
    id: createMeetingDialog
    
    property alias meetingTitle: titleField.text
    
    anchors.centerIn: parent
    width: 440
    title: "预约会议"
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
            text: "预约会议"
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
            
            onClicked: createMeetingDialog.close()
        }
    }
    
    contentItem: ColumnLayout {
        spacing: 16
        
        // 会议主题
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            
            Text {
                text: "会议主题"
                font.pixelSize: 14
                color: "#B0B0C0"
            }
            
            TextField {
                id: titleField
                Layout.fillWidth: true
                placeholderText: "请输入会议主题"
                text: meetingController ? meetingController.userName + "的会议" : "我的会议"
                font.pixelSize: 14
                leftPadding: 16
                rightPadding: 16
                
                background: Rectangle {
                    implicitHeight: 48
                    radius: 8
                    color: "#1A1A2E"
                    border.color: titleField.activeFocus ? "#1E90FF" : "#404060"
                    border.width: 1
                }
            }
        }
        
        // 开始时间
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                
                Text {
                    text: "开始日期"
                    font.pixelSize: 14
                    color: "#B0B0C0"
                }
                
                TextField {
                    id: dateField
                    Layout.fillWidth: true
                    placeholderText: "选择日期"
                    text: Qt.formatDate(new Date(), "yyyy-MM-dd")
                    font.pixelSize: 14
                    leftPadding: 16
                    rightPadding: 16
                    readOnly: true
                    
                    background: Rectangle {
                        implicitHeight: 44
                        radius: 8
                        color: "#1A1A2E"
                        border.color: "#404060"
                        border.width: 1
                    }
                }
            }
            
            ColumnLayout {
                Layout.preferredWidth: 120
                spacing: 8
                
                Text {
                    text: "开始时间"
                    font.pixelSize: 14
                    color: "#B0B0C0"
                }
                
                TextField {
                    id: timeField
                    Layout.fillWidth: true
                    placeholderText: "选择时间"
                    text: Qt.formatTime(new Date(), "hh:mm")
                    font.pixelSize: 14
                    leftPadding: 16
                    rightPadding: 16
                    readOnly: true
                    
                    background: Rectangle {
                        implicitHeight: 44
                        radius: 8
                        color: "#1A1A2E"
                        border.color: "#404060"
                        border.width: 1
                    }
                }
            }
        }
        
        // 会议时长
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            
            Text {
                text: "会议时长"
                font.pixelSize: 14
                color: "#B0B0C0"
            }
            
            ComboBox {
                id: durationCombo
                Layout.fillWidth: true
                model: ["30分钟", "1小时", "1.5小时", "2小时", "自定义"]
                currentIndex: 1
                font.pixelSize: 14
                
                background: Rectangle {
                    implicitHeight: 44
                    radius: 8
                    color: "#1A1A2E"
                    border.color: durationCombo.activeFocus ? "#1E90FF" : "#404060"
                    border.width: 1
                }
                
                contentItem: Text {
                    leftPadding: 16
                    text: durationCombo.displayText
                    font: durationCombo.font
                    color: "#FFFFFF"
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
        
        // 会议设置
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 12
            
            Text {
                text: "会议设置"
                font.pixelSize: 14
                color: "#B0B0C0"
            }
            
            CheckBox {
                id: waitingRoomCheck
                text: "开启等候室"
                font.pixelSize: 13
            }
            
            CheckBox {
                id: muteOnJoinCheck
                text: "参会者入会时静音"
                checked: true
                font.pixelSize: 13
            }
            
            CheckBox {
                id: recordCheck
                text: "自动录制会议"
                font.pixelSize: 13
            }
        }
        
        // 按钮
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 8
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
                
                onClicked: createMeetingDialog.close()
            }
            
            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                text: "创建会议"
                
                background: Rectangle {
                    radius: 8
                    color: parent.pressed ? "#1873CC" : 
                           parent.hovered ? "#4DA6FF" : "#1E90FF"
                }
                
                contentItem: Text {
                    text: parent.text
                    font.pixelSize: 14
                    font.bold: true
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                onClicked: createMeetingDialog.accept()
            }
        }
    }
    
    onOpened: {
        titleField.text = (meetingController ? meetingController.userName : "我") + "的会议"
        titleField.selectAll()
        titleField.forceActiveFocus()
    }
}
