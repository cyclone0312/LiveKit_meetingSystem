import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

/**
 * ConnectionLoadingDialog - 连接等待对话框
 * 
 * 在加入会议时显示，提示用户正在连接
 */
Dialog {
    id: connectionLoadingDialog
    
    anchors.centerIn: parent
    width: 300
    modal: true
    closePolicy: Popup.NoAutoClose  // 不允许自动关闭
    
    background: Rectangle {
        color: "#252542"
        radius: 12
    }
    
    contentItem: ColumnLayout {
        spacing: 24
        
        // 加载动画
        Item {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 60
            Layout.preferredHeight: 60
            
            // 旋转的加载圆圈
            Rectangle {
                id: loadingCircle
                anchors.centerIn: parent
                width: 48
                height: 48
                radius: 24
                color: "transparent"
                border.color: "#1E90FF"
                border.width: 4
                
                // 只显示部分边框模拟加载效果
                Rectangle {
                    anchors.centerIn: parent
                    width: 48
                    height: 48
                    radius: 24
                    color: "transparent"
                    border.color: "#252542"
                    border.width: 5
                    
                    // 遮挡 3/4 的圆
                    Rectangle {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        width: parent.width / 2
                        height: parent.height / 2
                        color: "#252542"
                    }
                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        width: parent.width / 2
                        height: parent.height / 2
                        color: "#252542"
                    }
                    Rectangle {
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        width: parent.width / 2
                        height: parent.height / 2
                        color: "#252542"
                    }
                }
                
                RotationAnimator {
                    target: loadingCircle
                    from: 0
                    to: 360
                    duration: 1000
                    loops: Animation.Infinite
                    running: connectionLoadingDialog.visible
                }
            }
        }
        
        // 提示文字
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "正在加入会议..."
            font.pixelSize: 16
            color: "#FFFFFF"
        }
        
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "请稍候"
            font.pixelSize: 14
            color: "#808090"
        }
    }
    
    // 监听连接状态变化
    Connections {
        target: liveKitManager
        
        function onConnected() {
            console.log("[ConnectionLoadingDialog] 连接成功，关闭等待对话框")
            connectionLoadingDialog.close()
        }
        
        function onErrorOccurred(error) {
            console.log("[ConnectionLoadingDialog] 连接出错，关闭等待对话框:", error)
            connectionLoadingDialog.close()
        }
    }
}
