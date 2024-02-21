#include "imageinfo.h"

int ImageInfo::DelayStandard = 200;				//标准延时，如果超过这个时间，就得抛弃帧或者等待音频帧

ImageInfo::ImageInfo(SDL_Renderer* renderer, SDL_Texture* texture, int width, int height, VoiceInfo* Voice, int Ypitch,
                     int Upitch, int Vpitch, int maxVector, QObject *parent)
    : QObject{parent}
{
    m_renderer = renderer;
    m_texture = texture;
    m_rect.x = 0;
    m_rect.y = 0;
    //默认值
    m_rect.w = width == 0 ? 640 : width;
    m_rect.h = height == 0 ? 480 : height;

    m_voice = Voice;
    mMaxVector = maxVector;
    mPushIndex = 0;
    mPlayIndex = 0;
    mImageNumber = 0;

    //分配缓存空间
    m_video_vector.resize(mMaxVector);
    sdlImageInfo* temp = nullptr;
    for (int i = 0; i < mMaxVector; ++i) {

        temp = new sdlImageInfo;
        temp->Yplane = new uint8_t[Ypitch * m_rect.h];
        temp->Ypitch = 0;
        temp->Uplane = new uint8_t[Upitch * m_rect.h / 2];
        temp->Upitch = 0;
        temp->Vplane = new uint8_t[Vpitch * m_rect.h / 2];
        temp->Vpitch = 0;
        temp->time_stamp = 0;
        m_video_vector[i] = temp;
    }

    m_video_vector_ptr.resize(mMaxVector);
    for (int i = 0; i < mMaxVector; ++i) {
        m_video_vector_ptr[i] = std::make_unique<sdlImageInfoPtr>();
        std::unique_ptr<uint8_t> tempY(new uint8_t[m_rect.h]);
        m_video_vector_ptr[i]->Yplane.reset(tempY.release());

        std::unique_ptr<uint8_t> tempU(new uint8_t[m_rect.h / 2]);
        m_video_vector_ptr[i]->Uplane.reset(tempU.release());

        std::unique_ptr<uint8_t> tempV(new uint8_t[m_rect.h / 2]);
        m_video_vector_ptr[i]->Vplane.reset(tempV.release());

        m_video_vector_ptr[i]->Ypitch = 0;
        m_video_vector_ptr[i]->Upitch = 0;
        m_video_vector_ptr[i]->Vpitch = 0;
    }

    mState = ImageState::Begin;	//开始状态
}

ImageInfo::~ImageInfo()
{
    //要析构了，图片队列数据要清空
    //要析构了，队列数据要清空
    //这里有点问题，清理到大概15个以后就会报HEAP: Free Heap block xxx modified at xxx after it was freed
    //为什么呢？
    //mState = ImageState::Stop;
    // sdlImageInfo* current_image = nullptr;
    // for (int i = 0; i < mMaxVector; ++i) {

    //     current_image = m_video_vector[i];

    //     delete[] current_image->Yplane;
    //     current_image->Yplane = nullptr;

    //     delete[] current_image->Uplane;
    //     current_image->Uplane = nullptr;

    //     delete[] current_image->Vplane;
    //     current_image->Vplane = nullptr;

    //     delete current_image;
    //     current_image = nullptr;
    // }
}

//注意这里复制内存需要乘以图片的高
bool ImageInfo::push(uint8_t* Yplane, int Ypitch, uint8_t* Uplane, int Upitch, uint8_t* Vplane, int Vpitch, long long time_stamp)
{
    //如果链表满了 就等待播放
    {
        std::lock_guard<std::mutex> mylockguard(m_mutex);
        while (m_video_vector[mPushIndex]->time_stamp != 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        memcpy(m_video_vector[mPushIndex]->Yplane, Yplane, Ypitch * m_rect.h);
        m_video_vector[mPushIndex]->Ypitch = Ypitch;
        memcpy(m_video_vector[mPushIndex]->Uplane, Uplane, Upitch * m_rect.h / 2);
        m_video_vector[mPushIndex]->Upitch = Upitch;
        memcpy(m_video_vector[mPushIndex]->Vplane, Vplane, Vpitch * m_rect.h / 2);
        m_video_vector[mPushIndex]->Vpitch = Vpitch;
        m_video_vector[mPushIndex]->time_stamp = time_stamp;


        // memcpy(m_video_vector_ptr[mPushIndex]->Yplane.get(), Yplane, Ypitch * m_rect.h);
        // m_video_vector_ptr[mPushIndex]->Ypitch = Ypitch;
        // memcpy(m_video_vector_ptr[mPushIndex]->Uplane.get(), Uplane, Upitch * m_rect.h / 2);
        // m_video_vector_ptr[mPushIndex]->Upitch = Upitch;
        // memcpy(m_video_vector_ptr[mPushIndex]->Vplane.get(), Vplane, Vpitch * m_rect.h / 2);
        // m_video_vector_ptr[mPushIndex]->Vpitch = Vpitch;
        // m_video_vector_ptr[mPushIndex]->time_stamp = time_stamp;


        ++mPushIndex;
        mPushIndex = mPushIndex == mMaxVector ? 0 : mPushIndex;
    }
    ++mImageNumber;
    //qDebug("image_push:%d\n", mImageNumber);
    return true;
}

bool ImageInfo::pop()
{
    return true;
}

void ImageInfo::play()
{
    while (mState == ImageState::Begin)
    {
        while (!m_video_vector[mPlayIndex]->time_stamp)
        {
            //等待帧
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        //播放:根据音频的时间戳来同步
        sdlImageInfo* current_image = m_video_vector[mPlayIndex];
        //qDebug("play_image_time:%lld\n", current_image->time_stamp);

        //通过渲染器更新纹理
        if (current_image->Ypitch > 0 && current_image->Upitch > 0 && current_image->Vpitch > 0 && current_image->time_stamp > 0) {
            SDL_UpdateYUVTexture(m_texture, NULL,
                                 current_image->Yplane, current_image->Ypitch,
                                 current_image->Uplane, current_image->Upitch,
                                 current_image->Vplane, current_image->Vpitch);
            SDL_RenderClear(m_renderer);

            //最后一个参数如果有值就是只显示那么多，否则是拉伸
            SDL_RenderCopy(m_renderer, m_texture, NULL, NULL);
            //SDL_RenderCopy(m_renderer, m_texture, NULL, &m_rect);

            SDL_RenderPresent(m_renderer);
        }

        //播放完毕，置0
        current_image->Ypitch = 0;
        current_image->Upitch = 0;
        current_image->Vpitch = 0;
        current_image->time_stamp = 0;

        ++mPlayIndex;
        mPlayIndex = mPlayIndex == mMaxVector ? 0 : mPlayIndex;
    }
}

SDL_Rect ImageInfo::getRect()
{
    return m_rect;
}

void ImageInfo::changeState(ImageState s)
{
    mState = s;
}

void ImageInfo::changeSDL(SDL_Renderer* newRenderer, SDL_Texture* newTexture)
{
    this->m_renderer = newRenderer;
    this->m_texture = newTexture;
}
