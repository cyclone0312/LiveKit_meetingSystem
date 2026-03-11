/**
 * @file videocompositor.h
 * @brief 视频合成器
 *
 * 负责：
 * 1. 接收多路视频帧（本地摄像头、远程参会者、屏幕共享）
 * 2. 将所有画面合成到一个 1920x1080 的画布上（网格布局）
 * 3. 叠加参会者姓名标签
 * 4. 30fps 定时器驱动输出合成帧
 */

#ifndef VIDEOCOMPOSITOR_H
#define VIDEOCOMPOSITOR_H

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QMap>
#include <QTimer>
#include <QRect>
#include <chrono>

class VideoCompositor : public QObject
{
    Q_OBJECT
public:
    explicit VideoCompositor(QObject *parent = nullptr);
    ~VideoCompositor() override;

    static constexpr int OUTPUT_WIDTH = 1920;
    static constexpr int OUTPUT_HEIGHT = 1080;
    static constexpr int OUTPUT_FPS = 30;

    /**
     * @brief 开始合成（启动定时器）
     */
    void start();

    /**
     * @brief 停止合成（停止定时器）
     */
    void stop();

    bool isRunning() const { return m_running; }

public slots:
    /**
     * @brief 输入参会者视频帧
     * @param participantId 参会者标识（"local" 代表本地摄像头，"screen" 代表屏幕共享）
     * @param frame BGRA/ARGB32 格式的 QImage
     * @param displayName 显示名称（叠加到画面上）
     */
    void feedFrame(const QString &participantId, const QImage &frame,
                   const QString &displayName = QString());

    /**
     * @brief 移除参会者（离开会议时）
     */
    void removeParticipant(const QString &participantId);

signals:
    /**
     * @brief 合成帧就绪
     * @param frame 1920x1080 的合成画面（Format_ARGB32）
     * @param timestampUs 时间戳（微秒）
     */
    void compositeFrameReady(const QImage &frame, qint64 timestampUs);

private slots:
    void onTimer();

private:
    /**
     * @brief 计算网格布局
     */
    void recalcLayout();

    /**
     * @brief 在画布上绘制名称标签
     */
    void drawNameLabel(QPainter &painter, const QRect &cell,
                       const QString &name);

private:
    QTimer *m_timer;
    bool m_running = false;

    mutable QMutex m_mutex;

    struct ParticipantFrame
    {
        QImage lastFrame; // 最近一帧（Format_ARGB32）
        QString displayName;
    };

    QMap<QString, ParticipantFrame> m_frames;

    // 布局缓存
    QMap<QString, QRect> m_layout; // participantId → cell rect
    int m_lastParticipantCount = 0;

    // 时间基准
    std::chrono::steady_clock::time_point m_startTime;
};

#endif // VIDEOCOMPOSITOR_H
