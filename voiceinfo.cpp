#include "voiceinfo.h"

#include <QDebug>


int VoiceInfo::DelayStandard = 200;                 //标准延时，如果超过这个时间，就得抛弃帧或者等待音频帧

VoiceInfo::VoiceInfo(int maxVector, int out_buffer_size, QObject *parent)
    : QObject{parent}
{
    //初始化音频开始
    AVSampleFormat initial_coded_sample = AV_SAMPLE_FMT_S16;                   //输出采样位
    int initial_nb_chnnels = 1;
    int initial_sample_rate = 8000;
    //本来这个值应该是AVCodecContext->framesize 但是rtsp中这个值是0，只能从AVframe->nb_samples中获取，
    //但这里是开多个窗口，不能多次初始化音频，所以之只能在最前面指定值了
    int out_nb_samples = 160;

    SDL_AudioSpec desired_audiospec;
    SDL_zero(desired_audiospec);
    desired_audiospec.format = AUDIO_FORMAT_MAP[initial_coded_sample];;
    desired_audiospec.channels = initial_nb_chnnels;
    desired_audiospec.freq = initial_sample_rate;
    desired_audiospec.samples = out_nb_samples;

    SDL_AudioSpec obtained_audiospec;
    SDL_zero(obtained_audiospec);

    m_audio_device_id = SDL_OpenAudioDevice(nullptr, false, &desired_audiospec, &obtained_audiospec, 0);
    if (m_audio_device_id <= 0) {
        printf("can not open SDL!\n");
        cout << SDL_GetError() << endl;
        return;
    }
    cout << "m_audio_device_id:" << m_audio_device_id << endl;

    // SDL2 audio devices start out paused, unpause it:
    SDL_PauseAudioDevice(m_audio_device_id, 0);
    cout << "Audio initialized: " << obtained_audiospec.freq << " Hz, " << obtained_audiospec.channels << " channels" << endl;

    m_current_play_time = -999999;	//加断点发现音视频开始时间戳为负几千，因此设置这么个初始时间
    mMaxVector = maxVector;
    mPushIndex = 0;
    mPlayIndex = 0;
    mVoiceNumber = 0;
    volume = true; //声音默认开启

    //分配缓存空间
    m_audio_vector.resize(mMaxVector);
    sdlAudioInfo* temp = nullptr;
    for (int i = 0; i < mMaxVector; ++i) {
        temp = new sdlAudioInfo;
        temp->data_pcm = new uint8_t[out_buffer_size];
        temp->data_len = 0;
        temp->time_stamp = 0;
        m_audio_vector[i] = temp;
    }

    mState = VoiceState::Begin;	//开始状态
}

bool VoiceInfo::getVolume()
{
    return  volume;
}

bool VoiceInfo::setVolume(bool flag)
{
    volume = flag;
    return volume;
}

VoiceInfo::~VoiceInfo()
{
    //要析构了，队列数据要清空
    //mState = VoiceState::Stop;
    sdlAudioInfo* current_audio = nullptr;
    for (int i = 0; i < mMaxVector; ++i) {
        current_audio = m_audio_vector[i];

        delete[] current_audio->data_pcm;
        current_audio->data_pcm = nullptr;

        delete current_audio;
        current_audio = nullptr;
    }
    SDL_CloseAudioDevice(m_audio_device_id);
}

bool VoiceInfo::push(uint8_t* outputBuffer, int out_buffer_size, long long time_stamp)
{
    //如果链表满了 就等待播放
    {
        //std::lock_guard<std::mutex> mylockguard(m_mutex);
        while (m_audio_vector[mPushIndex]->data_len != 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        memcpy(m_audio_vector[mPushIndex]->data_pcm, outputBuffer, out_buffer_size);
        m_audio_vector[mPushIndex]->data_len = out_buffer_size;
        m_audio_vector[mPushIndex]->time_stamp = time_stamp;

        ++mPushIndex;
        mPushIndex = mPushIndex == mMaxVector ? 0 : mPushIndex;
    }

    ++mVoiceNumber;
    //qDebug("voice_push:%d\n", mVoiceNumber);
    return true;
}

bool VoiceInfo::pop()
{
    return true;
}

long long VoiceInfo::getTime()
{
    return m_current_play_time;
}

void VoiceInfo::changeState(VoiceState s)
{
    mState = s;
}

void VoiceInfo::play()
{
    while (mState == VoiceState::Begin)
    {
        while (!m_audio_vector[mPlayIndex]->time_stamp)
        {
            //等待帧
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        //播放:直接播放
        sdlAudioInfo* current_voice = m_audio_vector[mPlayIndex];
        //qDebug("play_voice_time:%lld\n", current_voice->time_stamp);

        //这里只能这么干来减少延时了
        //qDebug()<<mVoiceNumber;
        //SDL_ClearQueuedAudio(m_audio_device_id);
        if (mVoiceNumber < 50 || mVoiceNumber % 300 == 0) {
            SDL_ClearQueuedAudio(m_audio_device_id);
        }
        if(volume){
            SDL_QueueAudio(m_audio_device_id, current_voice->data_pcm, current_voice->data_len);
        }
        m_current_play_time = current_voice->time_stamp;


        //播放完毕，置0
        current_voice->data_len = 0;
        current_voice->time_stamp = 0;

        ++mPlayIndex;
        mPlayIndex = mPlayIndex == mMaxVector ? 0 : mPlayIndex;
    }
}
