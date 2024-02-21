#ifndef IMAGEINFO_H
#define IMAGEINFO_H

#include <QObject>
#include "voiceinfo.h"

//YUV图片结构体，对应AVFrame中的data和linesize
struct sdlImageInfo
{
    uint8_t* Yplane;
    int Ypitch;
    uint8_t* Uplane;
    int Upitch;
    uint8_t* Vplane;
    int Vpitch;
    long long time_stamp;   //微秒
};

//智能指针的方式实现
struct sdlImageInfoPtr
{
    std::unique_ptr<uint8_t> Yplane;
    int Ypitch;
    std::unique_ptr<uint8_t> Uplane;
    int Upitch;
    std::unique_ptr<uint8_t> Vplane;
    int Vpitch;
    long long time_stamp;   //微秒
};

class ImageInfo : public QObject
{
    Q_OBJECT
public:
    enum class ImageState {Begin = 1, Stop = 0};
    explicit ImageInfo(SDL_Renderer* renderer, SDL_Texture* texture, int width, int height, VoiceInfo* Voice, int Ypitch,
                       int Upitch, int Vpitch, int maxVector, QObject *parent = nullptr);
    bool push(uint8_t* Yplane, int Ypitch, uint8_t* Uplane, int Upitch, uint8_t* Vplane, int Vpitch, long long time_stamp);
    bool pop();
    void play();
    void changeSDL(SDL_Renderer* newRenderer, SDL_Texture* newTexture);
    SDL_Rect getRect();
    void changeState(ImageState s);
    ~ImageInfo();

private:
    std::vector<sdlImageInfo*> m_video_vector;      //sdl图像队列
    std::vector<std::unique_ptr<sdlImageInfoPtr>> m_video_vector_ptr;      //sdl图像队列-智能指针的方式
    SDL_Renderer* m_renderer;                       //SDL渲染器
    SDL_Texture* m_texture;                         //SDL纹理
    SDL_Rect m_rect;                                //窗口矩阵
    VoiceInfo* m_voice;                             //视频对应的音频对象指针
    std::mutex m_mutex;                             //互斥锁
    int mMaxVector;                                 //最大缓存量
    int mPushIndex;
    int mPlayIndex;
    int mImageNumber;
    ImageState mState;                               //状态：这个是视频播放状态
    static int DelayStandard;

signals:

};

#endif // IMAGEINFO_H
