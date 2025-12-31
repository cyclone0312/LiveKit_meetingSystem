import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: videoGrid
    
    property int columns: calculateColumns()
    property int rows: calculateRows()
    
    function calculateColumns() {
        var count = participantModel.count
        if (count <= 1) return 1
        if (count <= 4) return 2
        if (count <= 9) return 3
        return 4
    }
    
    function calculateRows() {
        var count = participantModel.count
        if (count <= 1) return 1
        if (count <= 2) return 1
        if (count <= 4) return 2
        if (count <= 6) return 2
        if (count <= 9) return 3
        return Math.ceil(count / 4)
    }
    
    GridView {
        id: gridView
        anchors.fill: parent
        anchors.margins: 4
        
        cellWidth: width / columns
        cellHeight: height / rows
        
        model: participantModel
        
        delegate: VideoItem {
            width: gridView.cellWidth - 8
            height: gridView.cellHeight - 8
            
            participantName: model.name
            isMicOn: model.isMicOn
            isCameraOn: model.isCameraOn
            isHost: model.isHost
            isHandRaised: model.isHandRaised
            isScreenSharing: model.isScreenSharing
        }
        
        // 空状态
        Item {
            anchors.fill: parent
            visible: participantModel.count === 0
            
            Column {
                anchors.centerIn: parent
                spacing: 16
                
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "📹"
                    font.pixelSize: 64
                    opacity: 0.5
                }
                
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "等待参会者加入..."
                    font.pixelSize: 16
                    color: "#808090"
                }
            }
        }
    }
}
