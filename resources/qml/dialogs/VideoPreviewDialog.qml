import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtMultimedia

Dialog {
    id: videoPreviewDialog

    anchors.centerIn: parent
    width: 640
    height: 520
    modal: true
    standardButtons: Dialog.NoButton

    // 音频电平直接绑定 C++ 属性
    property real audioLevel: mediaCapture ? mediaCapture.audioLevel : 0.0

    onOpened: {
        // 刷新设备列表
        mediaCapture.refreshDevices()
        // 启动摄像头预览
        mediaCapture.startCamera()
        // 启动麦克风（用于音量检测）
        mediaCapture.startMicrophone()
        // 延迟绑定 VideoSink
        bindPreviewSinkTimer.start()
    }

    onClosed: {
        // 停止摄像头和麦克风
        mediaCapture.stopCamera()
        mediaCapture.stopMicrophone()
    }

    Timer {
        id: bindPreviewSinkTimer
        interval: 200
        repeat: false
        onTriggered: {
            if (mediaCapture && previewVideoOutput && previewVideoOutput.videoSink) {
                mediaCapture.bindVideoSink(previewVideoOutput.videoSink)
                console.log("[VideoPreviewDialog] VideoSink 已绑定")
            }
        }
    }

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
            text: "视频预览"
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

            onClicked: videoPreviewDialog.close()
        }
    }

    contentItem: ColumnLayout {
        spacing: 16

        // 视频预览区域
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 280
            radius: 8
            color: "#1A1A2E"
            clip: true

            VideoOutput {
                id: previewVideoOutput
                anchors.fill: parent
                fillMode: VideoOutput.PreserveAspectFit
            }

            // 无摄像头时的占位
            Text {
                anchors.centerIn: parent
                text: "📷 摄像头未开启"
                font.pixelSize: 16
                color: "#808090"
                visible: !mediaCapture.cameraActive
            }
        }

        // 麦克风音量指示器
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            RowLayout {
                spacing: 8

                Text {
                    text: "🎤"
                    font.pixelSize: 16
                }

                Text {
                    text: "麦克风"
                    font.pixelSize: 14
                    color: "#FFFFFF"
                }
            }

            // 音量条
            Rectangle {
                Layout.fillWidth: true
                height: 8
                radius: 4
                color: "#1A1A2E"

                Rectangle {
                    width: parent.width * videoPreviewDialog.audioLevel
                    height: parent.height
                    radius: 4
                    color: videoPreviewDialog.audioLevel > 0.7 ? "#FF5252" :
                           videoPreviewDialog.audioLevel > 0.3 ? "#FF9800" : "#4CAF50"

                    Behavior on width {
                        NumberAnimation { duration: 80 }
                    }
                }
            }
        }

        // 设备选择
        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            // 摄像头选择
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Text {
                    text: "摄像头"
                    font.pixelSize: 12
                    color: "#B0B0C0"
                }

                ComboBox {
                    id: cameraCombo
                    Layout.fillWidth: true
                    model: mediaCapture.availableCameras
                    currentIndex: mediaCapture.currentCameraIndex

                    background: Rectangle {
                        radius: 6
                        color: "#2D2D4A"
                        border.color: "#3D3D5C"
                        border.width: 1
                    }

                    contentItem: Text {
                        text: cameraCombo.displayText
                        font.pixelSize: 13
                        color: "#FFFFFF"
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 8
                        elide: Text.ElideRight
                    }

                    onCurrentIndexChanged: {
                        if (currentIndex >= 0 && currentIndex !== mediaCapture.currentCameraIndex) {
                            mediaCapture.currentCameraIndex = currentIndex
                            // 切换摄像头后重新启动并绑定
                            mediaCapture.startCamera()
                            bindPreviewSinkTimer.start()
                        }
                    }
                }
            }

            // 麦克风选择
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Text {
                    text: "麦克风"
                    font.pixelSize: 12
                    color: "#B0B0C0"
                }

                ComboBox {
                    id: micCombo
                    Layout.fillWidth: true
                    model: mediaCapture.availableMicrophones
                    currentIndex: mediaCapture.currentMicrophoneIndex

                    background: Rectangle {
                        radius: 6
                        color: "#2D2D4A"
                        border.color: "#3D3D5C"
                        border.width: 1
                    }

                    contentItem: Text {
                        text: micCombo.displayText
                        font.pixelSize: 13
                        color: "#FFFFFF"
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 8
                        elide: Text.ElideRight
                    }

                    onCurrentIndexChanged: {
                        if (currentIndex >= 0 && currentIndex !== mediaCapture.currentMicrophoneIndex) {
                            mediaCapture.currentMicrophoneIndex = currentIndex
                            mediaCapture.startMicrophone()
                        }
                    }
                }
            }
        }

        // 关闭按钮
        Button {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 120
            Layout.preferredHeight: 36
            text: "关闭"
            font.pixelSize: 14

            background: Rectangle {
                radius: 8
                color: parent.pressed ? "#1873CC" : parent.hovered ? "#2196F3" : "#1E90FF"
            }

            contentItem: Text {
                text: parent.text
                font: parent.font
                color: "white"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            onClicked: videoPreviewDialog.close()
        }
    }
}
