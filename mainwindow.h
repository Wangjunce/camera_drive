#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <stdlib.h>
#include <stdio.h>
#include <SDL.h>
#include <QApplication>
#include <QLabel>
#include <QTimer>
#include <map>
#include <thread>
#include <string>
#include <vector>
#include <mutex>

// 需要使用C来对C++进行支持
// 注意注意注意，这里的C是大写的！不是小写的！小写会报错！
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

#include "imageinfo.h"
#include "sdllabel.h"

#define MAXVOICEVECTOR 50

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    enum class mkfEnum{
        active = 1,
        noactive = 0
    };
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    //初始化SDL
    void initialSDL();
    void startMain(SdlLabel* sdlLabel, MainWindow* w);
    //解码线程
    void decodeThread(AVFormatContext* fmtCtx, int videoIndex, int audioIndex, AVCodecContext* codecCtxVideo,
                      AVCodecContext* codecCtxAudio, float videoTimeBase, float audioTimeBase, int w_width, int w_height,
                      SdlLabel* sdlLabel);

    void playVideo(SdlLabel* sdlLabel, ImageInfo* image);
    void playAudio(SdlLabel* sdlLabel, VoiceInfo* voice);
    QString getMkfStreamName(){return mkfStreamName;};
    DWORD getMkfPid(){return mkfPid;};
    void closeMKF();
    std::map<QString, SdlLabel*> getSdlLabelList(){return mapSSRC;};
signals:
    void initialSDLwin_s(SdlLabel* l, int w_width, int w_height);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    //初始化SDL的window,必须要用信号槽机制，否则会出现gui不在主线程的报错
    void initialSDLwin(SdlLabel* l, int w_width, int w_height);

    void on_toolButton_refresh_clicked();

    //点击任意在线摄像头触发
    void startOneSDL();

    //开始播放或者是停止播放按钮被点击
    void startThreadClicked();
    void on_pushButtonMKF_clicked();

    void mkfProcessDetect();

private:
    Ui::MainWindow *ui;
    std::string ssrc_ip;

    std::map<QString, SdlLabel*> mapSSRC;
    std::mutex m_mutex;

    mkfEnum mkfStatus;
    DWORD mkfPid;
    QString mkfStreamName;
    //推流的windows窗口句柄
    SHELLEXECUTEINFO lpExecInfo{};

    QTimer* timer;
};

#endif // MAINWINDOW_H
