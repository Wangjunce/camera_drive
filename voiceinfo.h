#ifndef VOICEINFO_H
#define VOICEINFO_H

#include <QObject>
#include <vector>
#include <SDL.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>

extern "C"
{
//avcodec:编解码(最重要的库)
#include <libavcodec/avcodec.h>
    //avformat:封装格式处理
#include <libavformat/avformat.h>
    //swscale:视频像素数据格式转换
#include <libswscale/swscale.h>
    //avdevice:各种设备的输入输出
#include <libavdevice/avdevice.h>
    //avutil:工具库（大部分库都需要这个库的支持）
#include <libavutil/avutil.h>

#include <libswresample/swresample.h>

#include <libavutil/imgutils.h>

#include <libavutil/time.h>
}

using std::cout;
using std::endl;
struct sdlAudioInfo
{
    uint8_t* data_pcm;
    int data_len;
    long long time_stamp;
};

//  audio sample rates map from FFMPEG to SDL (only non planar)
static std::map<int, int> AUDIO_FORMAT_MAP = {
    // AV_SAMPLE_FMT_NONE = -1,
    {AV_SAMPLE_FMT_U8,  AUDIO_U8    },
    {AV_SAMPLE_FMT_S16, AUDIO_S16SYS},
    {AV_SAMPLE_FMT_S32, AUDIO_S32SYS},
    {AV_SAMPLE_FMT_FLT, AUDIO_F32SYS}
};

class VoiceInfo : public QObject
{
    Q_OBJECT
public:
    enum class VoiceState { Begin = 1, Stop = 0 };
    explicit VoiceInfo(int maxVector, int out_buffer_size, QObject *parent = nullptr);
    bool push(uint8_t* outputBuffer, int out_buffer_size, long long time_stamp);
    bool pop();
    long long getTime();
    void play();
    bool getVolume();
    bool setVolume(bool flag);
    void changeState(VoiceState s);
    ~VoiceInfo();

private:
    std::vector<sdlAudioInfo*> m_audio_vector;      //待播放音频队列
    SDL_AudioDeviceID m_audio_device_id;            //播放设备id
    long long m_current_play_time;                  //音频播放当前时刻，代表上一帧音频播放的timestamp，用这个来实现音视频同步
    std::mutex m_mutex;                             //互斥锁
    int mMaxVector;                                 //最大缓存量
    int mPushIndex;
    int mPlayIndex;
    int mVoiceNumber;
    bool volume;
    VoiceState mState;                               //状态：这个是声音播放状态
    static int DelayStandard;

signals:

};

#endif // VOICEINFO_H
