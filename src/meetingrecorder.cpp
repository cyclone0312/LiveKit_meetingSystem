/**
 * @file meetingrecorder.cpp
 * @brief 会议视频录制器实现
 */

#include "meetingrecorder.h"
#include <QDebug>
#include <QDir>
#include <QTimer>
#include <cstring>

MeetingRecorder::MeetingRecorder(QObject *parent) : QObject(parent)
{
    qDebug() << "[MeetingRecorder] 初始化完成";
}

MeetingRecorder::~MeetingRecorder()
{
    if (m_recording.load())
    {
        stopRecording();
    }
    qDebug() << "[MeetingRecorder] 销毁";
}

bool MeetingRecorder::startRecording(const QString &outputPath, int width,
                                     int height, int fps,
                                     int audioSampleRate)
{
    if (m_recording.load())
    {
        qWarning() << "[MeetingRecorder] 已在录制中";
        return false;
    }

    m_outputPath = outputPath;
    m_videoWidth = width;
    m_videoHeight = height;
    m_videoFps = fps;
    m_audioSampleRate = audioSampleRate;

    // 确保输出目录存在
    QDir().mkpath(QFileInfo(outputPath).absolutePath());

    if (!initFFmpeg(outputPath, width, height, fps, audioSampleRate))
    {
        emit errorOccurred("FFmpeg 初始化失败");
        return false;
    }

    m_videoFrameCount = 0;
    m_lastVideoPts = -1;
    m_audioSampleCount = 0;
    m_durationSeconds.store(0);
    m_startTimeUs = 0;
    m_audioTimeInitialized = false;
    m_wallClock.start();

    // 清空队列
    {
        QMutexLocker locker(&m_videoMutex);
        m_videoQueue.clear();
    }
    {
        QMutexLocker locker(&m_audioMutex);
        m_audioBuffer.clear();
        m_audioEncodeBuf.clear();
    }

    m_recording.store(true);
    emit recordingChanged();

    // 在独立线程中运行编码循环
    m_encodingThread = QThread::create([this]()
                                       { encodingLoop(); });
    m_encodingThread->start();

    qDebug() << "[MeetingRecorder] 开始录制:" << outputPath
             << width << "x" << height << "@" << fps << "fps";
    return true;
}

void MeetingRecorder::stopRecording()
{
    if (!m_recording.load())
        return;

    qDebug() << "[MeetingRecorder] 停止录制...";
    m_recording.store(false);

    // 唤醒编码线程
    m_encodeCondition.wakeAll();

    // 等待编码线程结束
    if (m_encodingThread)
    {
        m_encodingThread->wait(10000); // 最多等 10 秒
        delete m_encodingThread;
        m_encodingThread = nullptr;
    }

    // flush 编码器并关闭文件
    flushEncoders();
    cleanupFFmpeg();

    emit recordingChanged();
    emit recordingStopped(m_outputPath);

    qDebug() << "[MeetingRecorder] 录制已停止, 文件:" << m_outputPath
             << "时长:" << m_durationSeconds.load() << "秒";
}

void MeetingRecorder::feedVideoFrame(const QImage &frame, qint64 timestampUs)
{
    Q_UNUSED(timestampUs)
    if (!m_recording.load())
        return;

    // 使用统一挂钟时间戳，确保与音频共享同一时间原点
    qint64 wallTimeUs = m_wallClock.nsecsElapsed() / 1000;

    QMutexLocker locker(&m_videoMutex);
    // 限制队列长度，防止编码跟不上时堆积
    if (m_videoQueue.size() < 60)
    {
        m_videoQueue.enqueue({frame, wallTimeUs});
    }
    m_encodeCondition.wakeOne();
}

void MeetingRecorder::feedAudioData(const QByteArray &pcmData, int sampleRate,
                                    int channels)
{
    Q_UNUSED(sampleRate)
    Q_UNUSED(channels)
    if (!m_recording.load() || pcmData.isEmpty())
        return;

    QMutexLocker locker(&m_audioMutex);

    // 首次音频到达：用挂钟时间初始化音频 PTS 起点，与视频对齐
    if (!m_audioTimeInitialized)
    {
        qint64 wallTimeUs = m_wallClock.nsecsElapsed() / 1000;
        m_audioSampleCount =
            wallTimeUs * m_audioSampleRate / 1000000;
        m_audioTimeInitialized = true;
        qDebug() << "[MeetingRecorder] 首次音频到达, wallTime="
                 << wallTimeUs << "us, 初始 audioPts=" << m_audioSampleCount;
    }

    m_audioBuffer.append(pcmData);
    m_encodeCondition.wakeOne();
}

// ==============================================================================
// 编码线程主循环
// ==============================================================================

void MeetingRecorder::encodingLoop()
{
    qDebug() << "[MeetingRecorder] 编码线程启动";

    while (m_recording.load())
    {
        // 处理视频帧
        {
            QMutexLocker locker(&m_videoMutex);
            while (!m_videoQueue.isEmpty())
            {
                auto [frame, ts] = m_videoQueue.dequeue();
                locker.unlock();
                encodeVideoFrame(frame, ts);
                locker.relock();
            }
        }

        // 处理音频数据
        {
            QMutexLocker locker(&m_audioMutex);
            if (!m_audioBuffer.isEmpty())
            {
                QByteArray data = m_audioBuffer;
                m_audioBuffer.clear();
                locker.unlock();

                const auto *samples =
                    reinterpret_cast<const int16_t *>(data.constData());
                int sampleCount =
                    static_cast<int>(data.size()) / static_cast<int>(sizeof(int16_t));
                encodeAudioSamples(samples, sampleCount);
            }
        }

        // 更新时长（基于挂钟）
        {
            int seconds = static_cast<int>(m_wallClock.elapsed() / 1000);
            if (seconds != m_durationSeconds.load())
            {
                m_durationSeconds.store(seconds);
                QMetaObject::invokeMethod(this, [this]()
                                          { emit durationChanged(); }, Qt::QueuedConnection);
            }
        }

        // 等待新数据
        QMutexLocker condLocker(&m_conditionMutex);
        if (m_recording.load())
        {
            m_encodeCondition.wait(&m_conditionMutex, 30); // 最多等 30ms
        }
    }

    // 处理剩余数据
    {
        QMutexLocker locker(&m_videoMutex);
        while (!m_videoQueue.isEmpty())
        {
            auto [frame, ts] = m_videoQueue.dequeue();
            locker.unlock();
            encodeVideoFrame(frame, ts);
            locker.relock();
        }
    }
    {
        QMutexLocker locker(&m_audioMutex);
        if (!m_audioBuffer.isEmpty())
        {
            QByteArray data = m_audioBuffer;
            m_audioBuffer.clear();
            locker.unlock();

            const auto *samples =
                reinterpret_cast<const int16_t *>(data.constData());
            int sampleCount =
                static_cast<int>(data.size()) / static_cast<int>(sizeof(int16_t));
            encodeAudioSamples(samples, sampleCount);
        }
    }

    qDebug() << "[MeetingRecorder] 编码线程结束, 视频帧:" << m_videoFrameCount
             << "音频样本:" << m_audioSampleCount;
}

// ==============================================================================
// FFmpeg 初始化
// ==============================================================================

bool MeetingRecorder::initFFmpeg(const QString &outputPath, int width,
                                 int height, int fps, int audioSampleRate)
{
    int ret;

    // 创建输出格式上下文
    ret = avformat_alloc_output_context2(&m_formatCtx, nullptr, nullptr,
                                         outputPath.toUtf8().constData());
    if (ret < 0 || !m_formatCtx)
    {
        qWarning() << "[MeetingRecorder] avformat_alloc_output_context2 失败";
        return false;
    }

    // ==================== 视频流 ====================
    const AVCodec *videoCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!videoCodec)
    {
        qWarning() << "[MeetingRecorder] 未找到 H.264 编码器";
        cleanupFFmpeg();
        return false;
    }

    m_videoStream = avformat_new_stream(m_formatCtx, nullptr);
    if (!m_videoStream)
    {
        cleanupFFmpeg();
        return false;
    }

    m_videoCodecCtx = avcodec_alloc_context3(videoCodec);
    m_videoCodecCtx->codec_id = AV_CODEC_ID_H264;
    m_videoCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    m_videoCodecCtx->width = width;
    m_videoCodecCtx->height = height;
    m_videoCodecCtx->time_base = {1, VIDEO_TIME_BASE};
    m_videoCodecCtx->framerate = {fps, 1};
    m_videoCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    m_videoCodecCtx->gop_size = fps * 2; // 每 2 秒一个关键帧
    m_videoCodecCtx->max_b_frames = 0;   // 简化: 不使用 B 帧

    // libx264 preset
    av_opt_set(m_videoCodecCtx->priv_data, "preset", "fast", 0);
    av_opt_set(m_videoCodecCtx->priv_data, "tune", "zerolatency", 0);

    // CRF 模式，质量 23（默认）
    m_videoCodecCtx->bit_rate = 0;
    av_opt_set(m_videoCodecCtx->priv_data, "crf", "23", 0);

    if (m_formatCtx->oformat->flags & AVFMT_GLOBALHEADER)
    {
        m_videoCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    ret = avcodec_open2(m_videoCodecCtx, videoCodec, nullptr);
    if (ret < 0)
    {
        qWarning() << "[MeetingRecorder] 视频编码器打开失败:" << ret;
        cleanupFFmpeg();
        return false;
    }

    ret = avcodec_parameters_from_context(m_videoStream->codecpar,
                                          m_videoCodecCtx);
    if (ret < 0)
    {
        cleanupFFmpeg();
        return false;
    }
    m_videoStream->time_base = m_videoCodecCtx->time_base;

    // 分配视频帧（YUV420P）
    m_videoFrame = av_frame_alloc();
    m_videoFrame->format = AV_PIX_FMT_YUV420P;
    m_videoFrame->width = width;
    m_videoFrame->height = height;
    av_frame_get_buffer(m_videoFrame, 0);

    // 创建 SWS 上下文（BGRA → YUV420P）
    m_swsCtx = sws_getContext(width, height, AV_PIX_FMT_BGRA, width, height,
                              AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR, nullptr,
                              nullptr, nullptr);
    if (!m_swsCtx)
    {
        qWarning() << "[MeetingRecorder] sws_getContext 失败";
        cleanupFFmpeg();
        return false;
    }

    // ==================== 音频流 ====================
    const AVCodec *audioCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!audioCodec)
    {
        qWarning() << "[MeetingRecorder] 未找到 AAC 编码器";
        cleanupFFmpeg();
        return false;
    }

    m_audioStream = avformat_new_stream(m_formatCtx, nullptr);
    if (!m_audioStream)
    {
        cleanupFFmpeg();
        return false;
    }

    m_audioCodecCtx = avcodec_alloc_context3(audioCodec);
    m_audioCodecCtx->codec_id = AV_CODEC_ID_AAC;
    m_audioCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
    m_audioCodecCtx->sample_rate = audioSampleRate;
    m_audioCodecCtx->sample_fmt = AV_SAMPLE_FMT_FLTP; // AAC 需要 float planar
    m_audioCodecCtx->ch_layout = AV_CHANNEL_LAYOUT_MONO;
    m_audioCodecCtx->bit_rate = 128000;
    m_audioCodecCtx->time_base = {1, audioSampleRate};

    if (m_formatCtx->oformat->flags & AVFMT_GLOBALHEADER)
    {
        m_audioCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    ret = avcodec_open2(m_audioCodecCtx, audioCodec, nullptr);
    if (ret < 0)
    {
        qWarning() << "[MeetingRecorder] 音频编码器打开失败:" << ret;
        cleanupFFmpeg();
        return false;
    }

    ret = avcodec_parameters_from_context(m_audioStream->codecpar,
                                          m_audioCodecCtx);
    if (ret < 0)
    {
        cleanupFFmpeg();
        return false;
    }
    m_audioStream->time_base = m_audioCodecCtx->time_base;

    m_audioFrameSize = m_audioCodecCtx->frame_size;
    if (m_audioFrameSize <= 0)
        m_audioFrameSize = 1024; // AAC 默认 1024 samples/frame

    // 分配音频帧
    m_audioFrame = av_frame_alloc();
    m_audioFrame->format = AV_SAMPLE_FMT_FLTP;
    m_audioFrame->ch_layout = AV_CHANNEL_LAYOUT_MONO;
    m_audioFrame->sample_rate = audioSampleRate;
    m_audioFrame->nb_samples = m_audioFrameSize;
    av_frame_get_buffer(m_audioFrame, 0);

    // 创建 SWR 上下文（int16 → float planar）
    ret = swr_alloc_set_opts2(
        &m_swrCtx,
        &m_audioCodecCtx->ch_layout, AV_SAMPLE_FMT_FLTP, audioSampleRate,
        &m_audioCodecCtx->ch_layout, AV_SAMPLE_FMT_S16, audioSampleRate,
        0, nullptr);
    if (ret < 0 || !m_swrCtx)
    {
        qWarning() << "[MeetingRecorder] swr_alloc_set_opts2 失败";
        cleanupFFmpeg();
        return false;
    }
    swr_init(m_swrCtx);

    // ==================== 打开输出文件 ====================
    if (!(m_formatCtx->oformat->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&m_formatCtx->pb, outputPath.toUtf8().constData(),
                        AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            qWarning() << "[MeetingRecorder] avio_open 失败:" << outputPath;
            cleanupFFmpeg();
            return false;
        }
    }

    // 写文件头
    ret = avformat_write_header(m_formatCtx, nullptr);
    if (ret < 0)
    {
        qWarning() << "[MeetingRecorder] avformat_write_header 失败:" << ret;
        cleanupFFmpeg();
        return false;
    }

    qDebug() << "[MeetingRecorder] FFmpeg 初始化成功:"
             << width << "x" << height << "@" << fps
             << "audio:" << audioSampleRate << "Hz";
    return true;
}

void MeetingRecorder::cleanupFFmpeg()
{
    if (m_formatCtx && m_formatCtx->pb)
    {
        av_write_trailer(m_formatCtx);
    }

    if (m_swsCtx)
    {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }
    if (m_swrCtx)
    {
        swr_free(&m_swrCtx);
    }
    if (m_videoFrame)
    {
        av_frame_free(&m_videoFrame);
    }
    if (m_audioFrame)
    {
        av_frame_free(&m_audioFrame);
    }
    if (m_videoCodecCtx)
    {
        avcodec_free_context(&m_videoCodecCtx);
    }
    if (m_audioCodecCtx)
    {
        avcodec_free_context(&m_audioCodecCtx);
    }
    if (m_formatCtx)
    {
        if (m_formatCtx->pb && !(m_formatCtx->oformat->flags & AVFMT_NOFILE))
        {
            avio_closep(&m_formatCtx->pb);
        }
        avformat_free_context(m_formatCtx);
        m_formatCtx = nullptr;
    }
}

// ==============================================================================
// 编码
// ==============================================================================

bool MeetingRecorder::encodeVideoFrame(const QImage &frame,
                                       qint64 timestampUs)
{
    if (!m_videoCodecCtx || !m_formatCtx)
        return false;

    // 确保帧格式正确
    QImage bgraFrame;
    if (frame.format() == QImage::Format_ARGB32 ||
        frame.format() == QImage::Format_ARGB32_Premultiplied)
    {
        // QImage ARGB32 在内存中实际上是 BGRA（小端）
        bgraFrame = frame;
    }
    else
    {
        bgraFrame = frame.convertToFormat(QImage::Format_ARGB32);
    }

    // 缩放到目标分辨率（如果需要）
    if (bgraFrame.width() != m_videoWidth ||
        bgraFrame.height() != m_videoHeight)
    {
        bgraFrame =
            bgraFrame.scaled(m_videoWidth, m_videoHeight, Qt::IgnoreAspectRatio,
                             Qt::SmoothTransformation);
    }

    // BGRA → YUV420P
    av_frame_make_writable(m_videoFrame);

    const uint8_t *srcData[1] = {bgraFrame.constBits()};
    int srcLinesize[1] = {static_cast<int>(bgraFrame.bytesPerLine())};

    sws_scale(m_swsCtx, srcData, srcLinesize, 0, m_videoHeight,
              m_videoFrame->data, m_videoFrame->linesize);

    // 使用挂钟时间戳计算 PTS（time_base = 1/90000），避免低精度导致 DTS 重复
    int64_t pts = timestampUs * VIDEO_TIME_BASE / 1000000;
    // 保证 PTS 严格单调递增，避免 "non monotonically increasing dts" 错误
    if (pts <= m_lastVideoPts)
    {
        // 仅递增 1 tick，使下一帧的真实 PTS 能自然超越、避免级联偏移
        qDebug() << "[MeetingRecorder] Video PTS 碰撞, 原:" << pts
                 << "→" << (m_lastVideoPts + 1);
        pts = m_lastVideoPts + 1;
    }
    m_lastVideoPts = pts;
    m_videoFrame->pts = pts;
    m_videoFrameCount++;

    // 发送帧到编码器
    int ret = avcodec_send_frame(m_videoCodecCtx, m_videoFrame);
    if (ret < 0)
    {
        qWarning() << "[MeetingRecorder] avcodec_send_frame(video) 失败:" << ret;
        return false;
    }

    // 读取编码后的数据包
    AVPacket *pkt = av_packet_alloc();
    while (ret >= 0)
    {
        ret = avcodec_receive_packet(m_videoCodecCtx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        if (ret < 0)
        {
            av_packet_free(&pkt);
            return false;
        }

        // 时间基转换
        av_packet_rescale_ts(pkt, m_videoCodecCtx->time_base,
                             m_videoStream->time_base);
        pkt->stream_index = m_videoStream->index;

        ret = av_interleaved_write_frame(m_formatCtx, pkt);
        if (ret < 0)
        {
            qWarning() << "[MeetingRecorder] av_interleaved_write_frame(video) 失败";
        }
    }
    av_packet_free(&pkt);
    return true;
}

bool MeetingRecorder::encodeAudioSamples(const int16_t *samples,
                                         int sampleCount)
{
    if (!m_audioCodecCtx || !m_formatCtx || sampleCount <= 0)
        return false;

    // 追加到编码缓冲区
    m_audioEncodeBuf.append(reinterpret_cast<const char *>(samples),
                            sampleCount * static_cast<int>(sizeof(int16_t)));

    const int frameSizeBytes =
        m_audioFrameSize * static_cast<int>(sizeof(int16_t));

    // 每凑齐一帧就编码
    while (m_audioEncodeBuf.size() >= frameSizeBytes)
    {
        av_frame_make_writable(m_audioFrame);
        m_audioFrame->nb_samples = m_audioFrameSize;

        // 使用 SWR 将 int16 转为 float planar
        const uint8_t *inData[1] = {
            reinterpret_cast<const uint8_t *>(m_audioEncodeBuf.constData())};

        int converted = swr_convert(m_swrCtx, m_audioFrame->data,
                                    m_audioFrameSize, inData, m_audioFrameSize);
        if (converted < 0)
        {
            qWarning() << "[MeetingRecorder] swr_convert 失败";
            m_audioEncodeBuf.remove(0, frameSizeBytes);
            continue;
        }

        m_audioFrame->pts = m_audioSampleCount;
        m_audioSampleCount += m_audioFrameSize;

        int ret = avcodec_send_frame(m_audioCodecCtx, m_audioFrame);
        if (ret < 0)
        {
            qWarning() << "[MeetingRecorder] avcodec_send_frame(audio) 失败:" << ret;
            m_audioEncodeBuf.remove(0, frameSizeBytes);
            continue;
        }

        AVPacket *pkt = av_packet_alloc();
        while (ret >= 0)
        {
            ret = avcodec_receive_packet(m_audioCodecCtx, pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            if (ret < 0)
            {
                av_packet_free(&pkt);
                m_audioEncodeBuf.remove(0, frameSizeBytes);
                return false;
            }

            av_packet_rescale_ts(pkt, m_audioCodecCtx->time_base,
                                 m_audioStream->time_base);
            pkt->stream_index = m_audioStream->index;

            ret = av_interleaved_write_frame(m_formatCtx, pkt);
        }
        av_packet_free(&pkt);
        m_audioEncodeBuf.remove(0, frameSizeBytes);
    }

    return true;
}

void MeetingRecorder::flushEncoders()
{
    if (!m_formatCtx)
        return;

    // Flush 视频编码器
    if (m_videoCodecCtx)
    {
        avcodec_send_frame(m_videoCodecCtx, nullptr);
        AVPacket *pkt = av_packet_alloc();
        int ret = 0;
        while (ret >= 0)
        {
            ret = avcodec_receive_packet(m_videoCodecCtx, pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            if (ret < 0)
                break;
            av_packet_rescale_ts(pkt, m_videoCodecCtx->time_base,
                                 m_videoStream->time_base);
            pkt->stream_index = m_videoStream->index;
            av_interleaved_write_frame(m_formatCtx, pkt);
        }
        av_packet_free(&pkt);
    }

    // Flush 音频编码器
    if (m_audioCodecCtx)
    {
        avcodec_send_frame(m_audioCodecCtx, nullptr);
        AVPacket *pkt = av_packet_alloc();
        int ret = 0;
        while (ret >= 0)
        {
            ret = avcodec_receive_packet(m_audioCodecCtx, pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            if (ret < 0)
                break;
            av_packet_rescale_ts(pkt, m_audioCodecCtx->time_base,
                                 m_audioStream->time_base);
            pkt->stream_index = m_audioStream->index;
            av_interleaved_write_frame(m_formatCtx, pkt);
        }
        av_packet_free(&pkt);
    }
}
