pragma Singleton
import QtQuick 2.15

QtObject {
    // 主色调
    readonly property color primaryColor: "#1E90FF"
    readonly property color primaryDarkColor: "#1873CC"
    readonly property color primaryLightColor: "#4DA6FF"
    
    // 背景色
    readonly property color backgroundColor: "#1A1A2E"
    readonly property color surfaceColor: "#252542"
    readonly property color cardColor: "#2D2D4A"
    
    // 文字颜色
    readonly property color textPrimary: "#FFFFFF"
    readonly property color textSecondary: "#B0B0C0"
    readonly property color textHint: "#707080"
    
    // 功能色
    readonly property color successColor: "#4CAF50"
    readonly property color warningColor: "#FF9800"
    readonly property color errorColor: "#F44336"
    readonly property color dangerColor: "#E53935"
    
    // 按钮颜色
    readonly property color buttonNormal: "#3D3D5C"
    readonly property color buttonHover: "#4D4D6C"
    readonly property color buttonPressed: "#2D2D4C"
    readonly property color buttonDisabled: "#2A2A40"
    
    // 图标颜色
    readonly property color iconActive: "#FFFFFF"
    readonly property color iconInactive: "#808090"
    readonly property color iconDanger: "#F44336"
    
    // 边框
    readonly property color borderColor: "#404060"
    readonly property int borderRadius: 8
    
    // 间距
    readonly property int spacingSmall: 4
    readonly property int spacingMedium: 8
    readonly property int spacingLarge: 16
    readonly property int spacingXLarge: 24
    
    // 字体大小
    readonly property int fontSizeSmall: 12
    readonly property int fontSizeMedium: 14
    readonly property int fontSizeLarge: 16
    readonly property int fontSizeXLarge: 20
    readonly property int fontSizeTitle: 24
    
    // 动画时长
    readonly property int animationDuration: 200
}
