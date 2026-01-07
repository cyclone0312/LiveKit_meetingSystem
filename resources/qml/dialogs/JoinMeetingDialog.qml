import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Dialog {
    id: joinMeetingDialog
    
    property alias meetingId: meetingIdField.text
    
    anchors.centerIn: parent
    width: 400
    title: "加入会议"
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
            text: "加入会议"
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
            
            onClicked: joinMeetingDialog.close()
        }
    }
    
    contentItem: ColumnLayout {
        spacing: 20
        
        // 会议号输入
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            
            Text {
                text: "会议号"
                font.pixelSize: 14
                color: "#B0B0C0"
            }
            
            TextField {
                id: meetingIdField
                Layout.fillWidth: true
                placeholderText: "请输入会议号"
                font.pixelSize: 14
                leftPadding: 16
                rightPadding: 16
                
                background: Rectangle {
                    implicitHeight: 48
                    radius: 8
                    color: "#1A1A2E"
                    border.color: meetingIdField.activeFocus ? "#1E90FF" : "#404060"
                    border.width: 1
                }
                
                validator: RegularExpressionValidator {
                    regularExpression: /[0-9]*/
                }
            }
        }
        
        // 密码输入（可选）
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            
            Text {
                text: "会议密码（可选）"
                font.pixelSize: 14
                color: "#B0B0C0"
            }
            
            TextField {
                id: passwordField
                Layout.fillWidth: true
                placeholderText: "请输入会议密码"
                echoMode: TextInput.Password
                font.pixelSize: 14
                leftPadding: 16
                rightPadding: 16
                
                background: Rectangle {
                    implicitHeight: 48
                    radius: 8
                    color: "#1A1A2E"
                    border.color: passwordField.activeFocus ? "#1E90FF" : "#404060"
                    border.width: 1
                }
            }
        }
        
        // 入会选项
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 12
            
            Text {
                text: "入会选项"
                font.pixelSize: 14
                color: "#B0B0C0"
            }
            
            RowLayout {
                spacing: 16
                
                CheckBox {
                    id: muteOnJoinCheck
                    text: "入会时静音"
                    checked: true
                    font.pixelSize: 13
                }
                
                CheckBox {
                    id: cameraOffCheck
                    text: "入会时关闭摄像头"
                    checked: false
                    font.pixelSize: 13
                }
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
                
                onClicked: joinMeetingDialog.close()
            }
            
            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                text: "加入会议"
                enabled: meetingIdField.text.length > 0
                
                background: Rectangle {
                    radius: 8
                    color: parent.enabled ? 
                           (parent.pressed ? "#1873CC" : 
                            parent.hovered ? "#4DA6FF" : "#1E90FF") : 
                           "#3D3D5C"
                }
                
                contentItem: Text {
                    text: parent.text
                    font.pixelSize: 14
                    font.bold: true
                    color: parent.enabled ? "white" : "#606070"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                onClicked: {
                    // 设置初始状态（通过属性赋值，而非调用 setter 方法）
                    meetingController.isMicOn = !muteOnJoinCheck.checked
                    meetingController.isCameraOn = !cameraOffCheck.checked
                    joinMeetingDialog.accept()
                }
            }
        }
    }
    
    onOpened: {
        meetingIdField.text = ""
        passwordField.text = ""
        meetingIdField.forceActiveFocus()
    }
}
