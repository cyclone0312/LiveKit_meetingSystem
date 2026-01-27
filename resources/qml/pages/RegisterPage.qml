import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15

Page {
    id: registerPage
    
    signal registerSuccess()
    signal goToLogin()
    
    // 连接 meetingController 注册结果信号
    Connections {
        target: meetingController
        
        function onRegisterSuccess() {
            registerButton.isRegistering = false
            // 注册成功后跳转回登录页
            goToLogin()
        }
        
        function onRegisterFailed(error) {
            registerButton.isRegistering = false
            errorText.text = error
        }
    }
    
    background: Rectangle {
        color: "#1A1A2E"
    }
    
    // 背景装饰
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#1A1A2E" }
            GradientStop { position: 1.0; color: "#16213E" }
        }
    }
    
    // 装饰圆圈
    Rectangle {
        width: 400
        height: 400
        radius: 200
        x: -100
        y: -100
        color: "#1E90FF"
        opacity: 0.1
    }
    
    Rectangle {
        width: 300
        height: 300
        radius: 150
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: -100
        anchors.bottomMargin: -100
        color: "#1E90FF"
        opacity: 0.1
    }
    
    // 注册卡片
    Rectangle {
        id: registerCard
        anchors.centerIn: parent
        width: 420
        height: 580
        radius: 16
        color: "#252542"
        
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 36
            spacing: 16
            
            // Logo和标题
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 80
                
                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 8
                    
                    // Logo图标
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: 60
                        height: 60
                        radius: 12
                        color: "#1E90FF"
                        
                        Text {
                            anchors.centerIn: parent
                            text: "会"
                            font.pixelSize: 28
                            font.bold: true
                            color: "white"
                        }
                    }
                    
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "创建账号"
                        font.pixelSize: 22
                        font.bold: true
                        color: "#FFFFFF"
                    }
                }
            }
            
            // 用户名输入
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                
                Text {
                    text: "用户名"
                    font.pixelSize: 13
                    color: "#B0B0C0"
                }
                
                TextField {
                    id: usernameField
                    Layout.fillWidth: true
                    placeholderText: "请输入用户名"
                    font.pixelSize: 14
                    leftPadding: 16
                    rightPadding: 16
                    
                    background: Rectangle {
                        implicitHeight: 44
                        radius: 8
                        color: "#1A1A2E"
                        border.color: usernameField.activeFocus ? "#1E90FF" : "#404060"
                        border.width: 1
                    }
                }
            }
            
            // 手机号/邮箱输入
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                
                Text {
                    text: "手机号/邮箱"
                    font.pixelSize: 13
                    color: "#B0B0C0"
                }
                
                TextField {
                    id: contactField
                    Layout.fillWidth: true
                    placeholderText: "请输入手机号或邮箱"
                    font.pixelSize: 14
                    leftPadding: 16
                    rightPadding: 16
                    
                    background: Rectangle {
                        implicitHeight: 44
                        radius: 8
                        color: "#1A1A2E"
                        border.color: contactField.activeFocus ? "#1E90FF" : "#404060"
                        border.width: 1
                    }
                }
            }
            
            // 密码输入
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                
                Text {
                    text: "密码"
                    font.pixelSize: 13
                    color: "#B0B0C0"
                }
                
                TextField {
                    id: passwordField
                    Layout.fillWidth: true
                    placeholderText: "请输入密码（至少6位）"
                    echoMode: TextInput.Password
                    font.pixelSize: 14
                    leftPadding: 16
                    rightPadding: 16
                    
                    background: Rectangle {
                        implicitHeight: 44
                        radius: 8
                        color: "#1A1A2E"
                        border.color: passwordField.activeFocus ? "#1E90FF" : "#404060"
                        border.width: 1
                    }
                }
            }
            
            // 确认密码
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                
                Text {
                    text: "确认密码"
                    font.pixelSize: 13
                    color: "#B0B0C0"
                }
                
                TextField {
                    id: confirmPasswordField
                    Layout.fillWidth: true
                    placeholderText: "请再次输入密码"
                    echoMode: TextInput.Password
                    font.pixelSize: 14
                    leftPadding: 16
                    rightPadding: 16
                    
                    background: Rectangle {
                        implicitHeight: 44
                        radius: 8
                        color: "#1A1A2E"
                        border.color: confirmPasswordField.activeFocus ? "#1E90FF" : 
                                      (confirmPasswordField.text.length > 0 && confirmPasswordField.text !== passwordField.text) ? "#F44336" : "#404060"
                        border.width: 1
                    }
                    
                    Keys.onReturnPressed: registerButton.clicked()
                }
                
                // 密码不匹配提示
                Text {
                    visible: confirmPasswordField.text.length > 0 && confirmPasswordField.text !== passwordField.text
                    text: "两次输入的密码不一致"
                    font.pixelSize: 11
                    color: "#F44336"
                }
            }
            
            // 用户协议
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                
                CheckBox {
                    id: agreeCheckBox
                    font.pixelSize: 12
                }
                
                Text {
                    text: "我已阅读并同意"
                    font.pixelSize: 12
                    color: "#808090"
                }
                
                Text {
                    text: "《用户服务协议》"
                    font.pixelSize: 12
                    color: "#1E90FF"
                    
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            // 打开用户协议
                        }
                    }
                }
            }
            
            Item { Layout.fillHeight: true }
            
            // 错误提示
            Text {
                id: errorText
                Layout.fillWidth: true
                visible: text.length > 0
                text: ""
                font.pixelSize: 12
                color: "#F44336"
                horizontalAlignment: Text.AlignHCenter
            }
            
            // 注册按钮
            Button {
                id: registerButton
                Layout.fillWidth: true
                Layout.preferredHeight: 46
                text: isRegistering ? "注册中..." : "注 册"
                font.pixelSize: 16
                font.bold: true
                enabled: canRegister() && !isRegistering
                
                property bool isRegistering: false
                
                function canRegister() {
                    return usernameField.text.trim().length > 0 &&
                           contactField.text.trim().length > 0 &&
                           passwordField.text.length >= 6 &&
                           confirmPasswordField.text === passwordField.text &&
                           agreeCheckBox.checked
                }
                
                background: Rectangle {
                    radius: 8
                    color: registerButton.enabled ? 
                           (registerButton.pressed ? "#1873CC" : 
                            registerButton.hovered ? "#4DA6FF" : "#1E90FF") : 
                           "#3D3D5C"
                    
                    Behavior on color {
                        ColorAnimation { duration: 150 }
                    }
                }
                
                contentItem: Text {
                    text: registerButton.text
                    font: registerButton.font
                    color: registerButton.enabled ? "white" : "#606070"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                onClicked: {
                    if (!canRegister()) {
                        if (!agreeCheckBox.checked) {
                            errorText.text = "请先同意用户服务协议"
                        }
                        return
                    }
                    
                    // 调用后端注册接口
                    errorText.text = ""
                    isRegistering = true
                    if (meetingController) {
                        meetingController.registerUser(usernameField.text, passwordField.text)
                    }
                }
            }
            
            // 返回登录
            RowLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter
                spacing: 4
                
                Text {
                    text: "已有账号?"
                    font.pixelSize: 13
                    color: "#808090"
                }
                
                Text {
                    text: "返回登录"
                    font.pixelSize: 13
                    color: "#1E90FF"
                    
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: goToLogin()
                    }
                }
            }
        }
    }
    
    // 版本信息
    Text {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 20
        text: "Version 1.0.0"
        font.pixelSize: 12
        color: "#606070"
    }
}
