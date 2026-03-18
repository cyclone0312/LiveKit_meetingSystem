import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: videoGrid

    property var mediaCaptureRef: mediaCapture
    property var screenCaptureRef: liveKitManager ? liveKitManager.screenCapture : null
    property var videoTiles: participantModel ? participantModel.videoTiles : []
    property int tileCount: videoTiles.length
    property string focusedTileId: ""
    property var focusedTile: findTileById(focusedTileId)
    property bool hasFocusedTile: focusedTile !== null
    property var gridTiles: hasFocusedTile ? [] : videoTiles

    function calculateColumns(count) {
        if (count <= 1) return 1
        if (count <= 4) return 2
        if (count <= 9) return 3
        return 4
    }

    function calculateRows(count) {
        if (count <= 1) return 1
        if (count <= 2) return 1
        if (count <= 4) return 2
        if (count <= 6) return 2
        if (count <= 9) return 3
        return Math.ceil(count / 4)
    }

    function findTileById(tileId) {
        if (!tileId) {
            return null
        }

        for (var i = 0; i < videoTiles.length; ++i) {
            if (videoTiles[i].tileId === tileId) {
                return videoTiles[i]
            }
        }

        return null
    }

    function remainingTiles() {
        var tiles = []
        for (var i = 0; i < videoTiles.length; ++i) {
            var tile = videoTiles[i]
            if (tile.tileId !== focusedTileId) {
                tiles.push(tile)
            }
        }
        return tiles
    }

    function toggleFocus(tileId) {
        focusedTileId = focusedTileId === tileId ? "" : tileId
    }

    onVideoTilesChanged: {
        if (focusedTileId !== "" && findTileById(focusedTileId) === null) {
            focusedTileId = ""
        }
    }

    GridView {
        id: gridView
        anchors.fill: parent
        anchors.margins: 4
        visible: !videoGrid.hasFocusedTile

        readonly property int gridColumns: videoGrid.calculateColumns(videoGrid.tileCount)
        readonly property int gridRows: videoGrid.calculateRows(videoGrid.tileCount)

        cellWidth: width / Math.max(gridColumns, 1)
        cellHeight: height / Math.max(gridRows, 1)
        model: videoGrid.gridTiles

        delegate: Item {
            width: gridView.cellWidth
            height: gridView.cellHeight

            required property var modelData

            VideoItem {
                anchors.fill: parent
                anchors.margins: 4

                tileId: modelData.tileId
                participantId: modelData.participantId
                participantName: modelData.name
                streamType: modelData.streamType
                streamLabel: modelData.streamLabel
                remoteRenderKey: modelData.renderKey
                isMicOn: modelData.isMicOn
                isHost: modelData.isHost
                isHandRaised: modelData.isHandRaised
                isStreamActive: modelData.streamActive
                isLocalUser: modelData.isLocal
                isFocused: videoGrid.focusedTileId === modelData.tileId
                mediaCapture: modelData.isLocal ? videoGrid.mediaCaptureRef : null
                screenCapture: modelData.isLocal ? videoGrid.screenCaptureRef : null

                onToggleFocusRequested: function(requestedTileId) {
                    videoGrid.toggleFocus(requestedTileId)
                }
            }
        }
    }

    Item {
        id: focusedLayout
        anchors.fill: parent
        anchors.margins: 4
        visible: videoGrid.hasFocusedTile

        readonly property var sideTiles: videoGrid.remainingTiles()
        readonly property int sideTileCount: sideTiles.length
        readonly property int sideWidth: sideTileCount > 0 ? Math.min(width * 0.28, 300) : 0

        Item {
            id: featuredTileContainer
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: sideBar.visible ? sideBar.left : parent.right
            anchors.rightMargin: sideBar.visible ? 10 : 0

            VideoItem {
                anchors.fill: parent

                tileId: videoGrid.focusedTile ? videoGrid.focusedTile.tileId : ""
                participantId: videoGrid.focusedTile ? videoGrid.focusedTile.participantId : ""
                participantName: videoGrid.focusedTile ? videoGrid.focusedTile.name : ""
                streamType: videoGrid.focusedTile ? videoGrid.focusedTile.streamType : "camera"
                streamLabel: videoGrid.focusedTile ? videoGrid.focusedTile.streamLabel : "摄像头"
                remoteRenderKey: videoGrid.focusedTile ? videoGrid.focusedTile.renderKey : ""
                isMicOn: videoGrid.focusedTile ? videoGrid.focusedTile.isMicOn : false
                isHost: videoGrid.focusedTile ? videoGrid.focusedTile.isHost : false
                isHandRaised: videoGrid.focusedTile ? videoGrid.focusedTile.isHandRaised : false
                isStreamActive: videoGrid.focusedTile ? videoGrid.focusedTile.streamActive : false
                isLocalUser: videoGrid.focusedTile ? videoGrid.focusedTile.isLocal : false
                isFocused: true
                mediaCapture: videoGrid.focusedTile && videoGrid.focusedTile.isLocal ? videoGrid.mediaCaptureRef : null
                screenCapture: videoGrid.focusedTile && videoGrid.focusedTile.isLocal ? videoGrid.screenCaptureRef : null

                onToggleFocusRequested: function(requestedTileId) {
                    videoGrid.toggleFocus(requestedTileId)
                }
            }
        }

        Rectangle {
            id: sideBar
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            width: focusedLayout.sideWidth
            radius: 10
            color: "#0F172ACC"
            border.color: "#FFFFFF14"
            border.width: 1
            visible: focusedLayout.sideTileCount > 0
        }

        Flickable {
            anchors.fill: sideBar
            anchors.margins: 8
            clip: true
            contentWidth: width
            contentHeight: sideColumn.height
            visible: sideBar.visible

            Column {
                id: sideColumn
                width: parent.width
                spacing: 8

                Repeater {
                    model: focusedLayout.sideTiles

                    delegate: Item {
                        width: sideColumn.width
                        height: Math.max(120, sideColumn.width * 0.62)

                        required property var modelData

                        VideoItem {
                            anchors.fill: parent

                            tileId: modelData.tileId
                            participantId: modelData.participantId
                            participantName: modelData.name
                            streamType: modelData.streamType
                            streamLabel: modelData.streamLabel
                            remoteRenderKey: modelData.renderKey
                            isMicOn: modelData.isMicOn
                            isHost: modelData.isHost
                            isHandRaised: modelData.isHandRaised
                            isStreamActive: modelData.streamActive
                            isLocalUser: modelData.isLocal
                            isFocused: false
                            mediaCapture: modelData.isLocal ? videoGrid.mediaCaptureRef : null
                            screenCapture: modelData.isLocal ? videoGrid.screenCaptureRef : null

                            onToggleFocusRequested: function(requestedTileId) {
                                videoGrid.toggleFocus(requestedTileId)
                            }
                        }
                    }
                }
            }
        }
    }

    Item {
        anchors.fill: parent
        visible: videoGrid.tileCount === 0

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
