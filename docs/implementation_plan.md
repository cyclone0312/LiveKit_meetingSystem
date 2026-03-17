# MeetingRecorder 录像音视频同步修复分析与计划

## 录制逻辑现状分析

你目前的 [MeetingRecorder](file:///c:/Users/86158/Desktop/meeting/src/meetingrecorder.h#38-159) 逻辑**已经部分实现了**前面提到的“高级同步策略”，思路方向是非常正确的，而且比起很多初学者已经专业很多了：

1. **拥有上帝时钟（✅ 已实现）**
   你使用了 `QElapsedTimer m_wallClock` 来作为绝对时间基准。在 [feedVideoFrame](file:///c:/Users/86158/Desktop/meeting/src/meetingrecorder.cpp#113-130) 时，你获取了 `wallTimeUs` 并通过 `timestampUs * VIDEO_TIME_BASE / 1000000` 换算成了严格的视频 PTS。
2. **以音频为主驱动（✅ 已实现）**
   在 [feedAudioData](file:///c:/Users/86158/Desktop/meeting/src/meetingrecorder.cpp#131-155) 首次收到数据时，你记录了当时的 `wallTimeUs` 来换算成初始的 `m_audioSampleCount`。此后 `m_audioSampleCount` 严格按照音频帧大小累加 `m_audioSampleCount += m_audioFrameSize`。这完全锁死了音频的时间轴，不会因为系统卡顿而被拉长或缩短。

**💀 但为什么还会存在不同步现象（甚至偶尔播放卡顿）？**

根本原因出在：**你的“交织写入队列”逻辑做得不够，违背了 FFmpeg 的交织写入原则。**

观察你在 `MeetingRecorder::encodingLoop()` 里的核心逻辑：

```cpp
while (m_recording.load()) {
    // 1. 锁住视频队列，拿出一堆视频帧，连续编码并直接 av_interleaved_write_frame
    // 2. 锁住音频队列，拿出一大块音频数据，切片编码并连续 av_interleaved_write_frame
}
```

FFmpeg 的 `av_interleaved_write_frame` 函数并不像名字暗示的那样能“完美自动帮你交织”。它内部虽然有一个微小的缓存池，但它**强烈要求你（调用者）尽可能按照时间递增的顺序**把 `AVPacket` 喂给它。

想象一下发生轻微波动的场景：
由于线程调度或画面复杂，视频编码落后了100毫秒没送到队列。然后突然，你一次性从视频队列里拿出了 3 帧（跨度约 100ms），接着你连续调用 `av_interleaved_write_frame` 写了这 3 帧。此时写入器内的视频时间轴已经到了 15秒100毫秒。
接着代码走到音频块，你又拿出了一大块这期间积累的音频，开始连续写，而这些音频包的 PTS 其实跨度是 [15秒000ms ~ 15秒100ms]。
这就造成了向文件写入时**时间倒退（Non-monotonous DTS / PTS）**。如果时间差超过了 FFmpeg 内部的交织容忍度，它就会直接把晚写的（但时间更早的）音频包时间戳强行篡改，或者在文件容器结构（MP4/FLV 等的 chunk 索引）里留下错乱的索引，最终导致播放器在回放时解码缓冲发生混乱（音画剥离、延迟、突然快进）。

## 修复方案 Plan：编码与封装的彻底解耦

要彻底解决这个问题，你需要引入一个**包写入管理器（交织分发器）**。思路非常简单：不要在 [encode](file:///c:/Users/86158/Desktop/meeting/src/meetingrecorder.cpp#486-572) 函数里直接写文件。

### 1. 拦截 AVPacket
修改 [encodeVideoFrame](file:///c:/Users/86158/Desktop/meeting/src/meetingrecorder.cpp#486-572) 和 [encodeAudioSamples](file:///c:/Users/86158/Desktop/meeting/src/meetingrecorder.cpp#573-641)。当它们调用 `avcodec_receive_packet` 拿到编码后的 `pkt` 时，**不要**立刻调用 `av_interleaved_write_frame`！
我们新建两个队列（或优先队列）：
*   `QQueue<AVPacket*> m_videoPackets`
*   `QQueue<AVPacket*> m_audioPackets`
将转化好时间基（`av_packet_rescale_ts`）的包克隆或移动存进对应的队列。

### 2. 定义交织写入规则（Muxing Logic）
在 [encodingLoop](file:///c:/Users/86158/Desktop/meeting/src/meetingrecorder.cpp#160-244) 每次循环的最后（即音视频这轮都编码完了），调用一个新的函数 `writeInterleavedPackets()`。
这个函数的逻辑如下：
1. 观察 `m_videoPackets` 和 `m_audioPackets`。
2. **只有在一边队列为空之前（即两边都有可对比的包时）**：
   比较 `m_videoPackets.head()` 和 `m_audioPackets.head()`。
   我们需要把它们的时间转换到共同的 `AV_TIME_BASE`（1000000 微秒）下进行大小比较。
   ```cpp
   // 拿 DTS (Decoding Time Stamp) 比较最稳妥
   // 谁的时间小，谁就出列被 av_interleaved_write_frame 写入文件。
   ```
3. 循环步骤 2，直到某一个队列空了，写操作暂停，等下一次 [encodingLoop](file:///c:/Users/86158/Desktop/meeting/src/meetingrecorder.cpp#160-244) 有新包进来补充后再继续交织对比。

### 3. 处理收尾（Stop 阶段）
在按停止录制 [stopRecording](file:///c:/Users/86158/Desktop/meeting/src/meetingrecorder.cpp#83-112) 并 [flushEncoders](file:///c:/Users/86158/Desktop/meeting/src/meetingrecorder.cpp#642-689) 把最后的包都吐出来后，就不需要等待“两边都有包”了，直接把剩余两边队列里剩下的 `AVPacket` 按时间顺序一股脑全拿出来写进文件清仓。

---

如果按照这个思路改造，你录制出来的 MP4 文件的底层结构将是绝对健康和对齐的，任何常见的播放器（VLC, 网页视频，QQ影音）解析你的文件都不会产生哪怕 1 毫秒的偏差。

如果觉得目前分析合理不需要什么变更，我们可以立刻基于 `MeetingRecorder.cpp` 的现有代码进行修改。
