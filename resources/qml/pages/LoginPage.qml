import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15

Page {
    id: loginPage
    
    signal loginSuccess()
    signal goToRegister()
    
    // 连接 meetingController 登录结果信号
    Connections {
        target: meetingController
        
        function onLoginSuccess() {
            // 如果勾选了记住密码，保存凭据
            if (rememberCheckBox.checked) {
                meetingController.saveCredentials(usernameField.text, passwordField.text)
            } else {
                meetingController.clearSavedCredentials()
            }
            loginPage.loginSuccess()
        }
        
        function onLoginFailed(reason) {
            // 显示错误提示
            console.log("登录失败:", reason)
        }
    }
    
    // 页面加载时恢复保存的凭据
    Component.onCompleted: {
        if (meetingController.hasRememberedPassword()) {
            usernameField.text = meetingController.getSavedUsername()
            passwordField.text = meetingController.getSavedPassword()
            rememberCheckBox.checked = true
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
    
    // 登录卡片
    Rectangle {
        id: loginCard
        anchors.centerIn: parent
        width: 400
        height: 480
        radius: 16
        color: "#252542"
        
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 40
            spacing: 20
            
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
                        text: "视频会议"
                        font.pixelSize: 24
                        font.bold: true
                        color: "#FFFFFF"
                    }
                }
            }
            
            // 用户名输入
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                
                Text {
                    text: "用户名"
                    font.pixelSize: 14
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
                        implicitHeight: 48
                        radius: 8
                        color: "#1A1A2E"
                        border.color: usernameField.activeFocus ? "#1E90FF" : "#404060"
                        border.width: 1
                    }
                }
            }
            
            // 密码输入
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                
                Text {
                    text: "密码"
                    font.pixelSize: 14
                    color: "#B0B0C0"
                }
                
                TextField {
                    id: passwordField
                    Layout.fillWidth: true
                    placeholderText: "请输入密码"
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
                    
                    Keys.onReturnPressed: loginButton.clicked()
                }
            }
            
            // 记住密码
            RowLayout {
                Layout.fillWidth: true
                
                CheckBox {
                    id: rememberCheckBox
                    text: "记住密码"
                    font.pixelSize: 13
                }
                
                Item { Layout.fillWidth: true }
                
                Text {
                    text: "忘记密码?"
                    font.pixelSize: 13
                    color: "#1E90FF"
                    
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            // 处理忘记密码
                        }
                    }
                }
            }
            
            Item { Layout.fillHeight: true }
            
            // 登录按钮
            Button {
                id: loginButton
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: "登 录"
                font.pixelSize: 16
                font.bold: true
                
                background: Rectangle {
                    radius: 8
                    color: loginButton.pressed ? "#1873CC" : 
                           loginButton.hovered ? "#4DA6FF" : "#1E90FF"
                    
                    Behavior on color {
                        ColorAnimation { duration: 150 }
                    }
                }
                
                contentItem: Text {
                    text: loginButton.text
                    font: loginButton.font
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                onClicked: {
                    if (usernameField.text.trim() === "") {
                        usernameField.focus = true
                        return
                    }
                    
                    // 调用登录，通过信号处理结果
                    meetingController.login(usernameField.text, passwordField.text)
                }
            }
            
            // 注册链接
            RowLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter
                spacing: 4
                
                Text {
                    text: "还没有账号?"
                    font.pixelSize: 13
                    color: "#808090"
                }
                
                Text {
                    text: "立即注册"
                    font.pixelSize: 13
                    color: "#1E90FF"
                    
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: goToRegister()
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
