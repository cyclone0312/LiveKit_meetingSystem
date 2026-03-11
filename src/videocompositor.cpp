/**
 * @file videocompositor.cpp
 * @brief 视频合成器实现
 */

#include "videocompositor.h"
#include <QDebug>
#include <QPainter>
#include <QFont>
#include <cmath>

VideoCompositor::VideoCompositor(QObject *parent) : QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::PreciseTimer);
    connect(m_timer, &QTimer::timeout, this, &VideoCompositor::onTimer);
    qDebug() << "[VideoCompositor] 初始化完成";
}

VideoCompositor::~VideoCompositor()
{
    stop();
    qDebug() << "[VideoCompositor] 销毁";
}

void VideoCompositor::start()
{
    if (m_running)
        return;
    m_running = true;
    m_startTime = std::chrono::steady_clock::now();
    m_timer->start(1000 / OUTPUT_FPS);
    qDebug() << "[VideoCompositor] 开始合成, FPS:" << OUTPUT_FPS;
}

void VideoCompositor::stop()
{
    if (!m_running)
        return;
    m_running = false;
    m_timer->stop();
    qDebug() << "[VideoCompositor] 停止合成";
}

void VideoCompositor::feedFrame(const QString &participantId,
                                const QImage &frame,
                                const QString &displayName)
{
    if (frame.isNull())
        return;

    QMutexLocker locker(&m_mutex);
    auto &pf = m_frames[participantId];
    // 统一转为 ARGB32 方便绘制
    if (frame.format() != QImage::Format_ARGB32)
        pf.lastFrame = frame.convertToFormat(QImage::Format_ARGB32);
    else
        pf.lastFrame = frame;
    if (!displayName.isEmpty())
        pf.displayName = displayName;
}

void VideoCompositor::removeParticipant(const QString &participantId)
{
    QMutexLocker locker(&m_mutex);
    m_frames.remove(participantId);
    m_layout.clear(); // 触发重新布局
    m_lastParticipantCount = 0;
    qDebug() << "[VideoCompositor] 移除参会者:" << participantId;
}

void VideoCompositor::onTimer()
{
    QMutexLocker locker(&m_mutex);

    const int n = m_frames.size();
    if (n == 0)
    {
        // 没有任何参会者帧，输出黑画面
        QImage black(OUTPUT_WIDTH, OUTPUT_HEIGHT, QImage::Format_ARGB32);
        black.fill(Qt::black);
        auto now = std::chrono::steady_clock::now();
        qint64 ts = std::chrono::duration_cast<std::chrono::microseconds>(
                        now - m_startTime)
                        .count();
        locker.unlock();
        emit compositeFrameReady(black, ts);
        return;
    }

    // 重新计算布局（仅在参会者数量变化时）
    if (n != m_lastParticipantCount)
    {
        recalcLayout();
        m_lastParticipantCount = n;
    }

    // 创建画布
    QImage canvas(OUTPUT_WIDTH, OUTPUT_HEIGHT, QImage::Format_ARGB32);
    canvas.fill(QColor(26, 26, 46)); // 匹配 UI 背景色 #1A1A2E

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // 绘制每个参会者的画面
    for (auto it = m_frames.constBegin(); it != m_frames.constEnd(); ++it)
    {
        const QString &pid = it.key();
        const ParticipantFrame &pf = it.value();

        if (!m_layout.contains(pid))
            continue;

        const QRect &cell = m_layout[pid];

        if (!pf.lastFrame.isNull())
        {
            // 等比缩放填充到单元格
            QImage scaled = pf.lastFrame.scaled(cell.size(), Qt::KeepAspectRatio,
                                                Qt::SmoothTransformation);
            // 居中绘制
            int dx = cell.x() + (cell.width() - scaled.width()) / 2;
            int dy = cell.y() + (cell.height() - scaled.height()) / 2;

            // 先画黑底（letterbox）
            painter.fillRect(cell, Qt::black);
            painter.drawImage(dx, dy, scaled);
        }
        else
        {
            painter.fillRect(cell, Qt::black);
        }

        // 绘制名称标签
        if (!pf.displayName.isEmpty())
        {
            drawNameLabel(painter, cell, pf.displayName);
        }
    }

    painter.end();

    auto now = std::chrono::steady_clock::now();
    qint64 ts = std::chrono::duration_cast<std::chrono::microseconds>(
                    now - m_startTime)
                    .count();

    locker.unlock();
    emit compositeFrameReady(canvas, ts);
}

void VideoCompositor::recalcLayout()
{
    m_layout.clear();

    const int n = m_frames.size();
    if (n == 0)
        return;

    // 检查是否有屏幕共享 → 给它分配主画面
    bool hasScreen = m_frames.contains("screen");

    if (hasScreen && n > 1)
    {
        // 屏幕共享模式：屏幕共享占左侧 75%，其余参会者在右侧 25% 纵向排列
        int mainW = OUTPUT_WIDTH * 3 / 4;
        int sideW = OUTPUT_WIDTH - mainW;

        m_layout["screen"] = QRect(0, 0, mainW, OUTPUT_HEIGHT);

        int otherCount = n - 1;
        int cellH = OUTPUT_HEIGHT / qMax(otherCount, 1);
        int idx = 0;
        for (auto it = m_frames.constBegin(); it != m_frames.constEnd(); ++it)
        {
            if (it.key() == "screen")
                continue;
            m_layout[it.key()] = QRect(mainW, idx * cellH, sideW, cellH);
            idx++;
        }
    }
    else
    {
        // 普通网格模式
        int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(n))));
        int rows = static_cast<int>(
            std::ceil(static_cast<double>(n) / static_cast<double>(cols)));
        int cellW = OUTPUT_WIDTH / cols;
        int cellH = OUTPUT_HEIGHT / rows;

        int idx = 0;
        for (auto it = m_frames.constBegin(); it != m_frames.constEnd(); ++it)
        {
            int row = idx / cols;
            int col = idx % cols;
            m_layout[it.key()] = QRect(col * cellW, row * cellH, cellW, cellH);
            idx++;
        }
    }
}

void VideoCompositor::drawNameLabel(QPainter &painter, const QRect &cell,
                                    const QString &name)
{
    // 半透明黑底 + 白色文字，位于单元格底部
    int labelH = 28;
    QRect labelRect(cell.x(), cell.bottom() - labelH, cell.width(), labelH);

    painter.fillRect(labelRect, QColor(0, 0, 0, 150));

    QFont font;
    font.setPixelSize(14);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(labelRect, Qt::AlignCenter, name);
}
