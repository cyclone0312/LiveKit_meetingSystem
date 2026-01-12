import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

/**
 * VideoGrid - 参会者视频网格布局
 * 
 * 【GridView Delegate 作用域问题】
 * GridView 的 delegate 是动态创建的组件，有独立的作用域。
 * delegate 内部无法直接访问全局变量（如 main.cpp 中 setContextProperty 注册的 mediaCapture）。
 * 
 * 解决方案：
 * 1. 在 VideoGrid 中定义属性 mediaCaptureRef 保存全局 mediaCapture 的引用
 * 2. delegate 通过 videoGrid.mediaCaptureRef 访问（通过父组件 id 访问）
 * 
 * 错误示例（会得到 null）：
 *   delegate: VideoItem { mediaCapture: mediaCapture }  // ❌ delegate 内访问全局变量失败
 * 
 * 正确示例：
 *   delegate: VideoItem { mediaCapture: videoGrid.mediaCaptureRef }  // ✅ 通过父组件属性传递
 */
Item {
    id: videoGrid
    
    // 【关键】将全局 mediaCapture 保存为本组件的属性
    // 这样 delegate 就可以通过 videoGrid.mediaCaptureRef 访问
    property var mediaCaptureRef: mediaCapture
    
    // 【新增】屏幕共享引用，供本地用户显示屏幕共享预览
    property var screenCaptureRef: liveKitManager ? liveKitManager.screenCapture : null
    
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
            id: videoItemDelegate
            width: gridView.cellWidth - 8
            height: gridView.cellHeight - 8
            
            participantId: model.participantId  // 传递参会者 ID
            participantName: model.name
            isMicOn: model.isLocal ? meetingController.isMicOn : model.isMicOn
            isCameraOn: model.isLocal ? meetingController.isCameraOn : model.isCameraOn
            isHost: model.isHost
            isHandRaised: model.isHandRaised
            // 【修复】本地用户的屏幕共享状态从 meetingController 获取
            isScreenSharing: model.isLocal ? meetingController.isScreenSharing : model.isScreenSharing
            isLocalUser: model.isLocal  // 标记是否为本地用户
            
            // 【重要】通过 videoGrid 的属性传递 mediaCapture，而非直接引用全局变量
            // 只有本地用户需要 mediaCapture 来显示本地摄像头画面
            mediaCapture: model.isLocal ? videoGrid.mediaCaptureRef : null
            
            // 【新增】传递 screenCapture 供本地屏幕共享预览
            screenCapture: model.isLocal ? videoGrid.screenCaptureRef : null
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
