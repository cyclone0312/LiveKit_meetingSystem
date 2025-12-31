import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Button {
    id: meetingButton
    
    property string iconText: ""
    property string labelText: ""
    property bool isActive: false
    property bool isDanger: false
    property int badgeCount: 0
    property color activeColor: "#1E90FF"
    
    implicitWidth: 72
    implicitHeight: 64
    
    background: Rectangle {
        radius: 8
        color: {
            if (meetingButton.isDanger && !meetingButton.isActive) {
                return meetingButton.pressed ? "#C62828" : 
                       meetingButton.hovered ? "#EF5350" : "#F44336"
            }
            if (meetingButton.isActive) {
                return meetingButton.pressed ? Qt.darker(activeColor, 1.2) : 
                       meetingButton.hovered ? Qt.lighter(activeColor, 1.1) : activeColor
            }
            return meetingButton.pressed ? "#2D2D4C" : 
                   meetingButton.hovered ? "#4D4D6C" : "#3D3D5C"
        }
        
        Behavior on color {
            ColorAnimation { duration: 150 }
        }
    }
    
    contentItem: Item {
        ColumnLayout {
            anchors.centerIn: parent
            spacing: 4
            
            // 图标
            Item {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                
                Text {
                    anchors.centerIn: parent
                    text: iconText
                    font.pixelSize: 20
                }
                
                // 角标
                Rectangle {
                    visible: badgeCount > 0
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.rightMargin: -8
                    anchors.topMargin: -4
                    width: Math.max(18, badgeText.width + 8)
                    height: 18
                    radius: 9
                    color: "#F44336"
                    
                    Text {
                        id: badgeText
                        anchors.centerIn: parent
                        text: badgeCount > 99 ? "99+" : badgeCount.toString()
                        font.pixelSize: 10
                        font.bold: true
                        color: "white"
                    }
                }
            }
            
            // 标签
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: labelText
                font.pixelSize: 11
                color: "white"
            }
        }
    }
}
