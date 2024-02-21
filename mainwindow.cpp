#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "utils.h"
#include <curl/curl.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <vector>
#include <algorithm>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ssrc_ip = RTPURL;

    mkfStatus = mkfEnum::noactive;
    mkfPid = 0;
    mkfStreamName = "";
    timer = new QTimer(this);
    timer->setInterval(5000);
    ui->setupUi(this);

    //音频设备初始化，由于音频只能打开一次，所以只能在最前面初始化
    this->initialSDL();

    QVBoxLayout* verticalLayout = this->findChild<QVBoxLayout*>("verticalLayout");
    QLabel* cameraTitle = new QLabel();
    cameraTitle->setText("摄像头列表");
    verticalLayout->addWidget(cameraTitle, 0, Qt::AlignLeft);
    verticalLayout->addStretch(9);

    QGridLayout* gridLayout = this->findChild<QGridLayout*>("gridLayout");
    // 设置行和列的数量
    int rowCount = 18; // 行的数量
    int columnCount = 9; // 列的数量
    // 设置行和列的大小调整策略
    for (int row = 0; row < rowCount; ++row) {
        gridLayout->setRowStretch(row, 1); // 行的大小调整策略和比例
    }
    for (int col = 0; col < columnCount; ++col) {
        gridLayout->setColumnStretch(col, 1); // 列的大小调整策略和比例
    }

    //这里全局函数没有办法设置BlockingQueuedConnection，这样的话初始化的时候会报gui线程不在主线程的错误
    //connect(this, SIGNAL(&MainWindow::initialSDLwin_s), "initialSDLwin", Qt::BlockingQueuedConnection);
    //connect(this, &MainWindow::initialSDLwin_s, ::initialSDLwin, Qt::BlockingQueuedConnection);
    connect(this, &MainWindow::initialSDLwin_s, this, &MainWindow::initialSDLwin, Qt::BlockingQueuedConnection);
    connect(timer, &QTimer::timeout, this, &MainWindow::mkfProcessDetect);

    //右上角刷新按钮
    QToolButton* r = this->findChild<QToolButton*>("toolButton_refresh");
    QIcon icon("icon/refresh.png");  // 替换为实际的图标文件路径
    r->setIcon(icon);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initialSDL()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not initialize SDL - %s\n", SDL_GetError());
        return;
    }
}

void MainWindow::initialSDLwin(SdlLabel* l, int w_width, int w_height)
{
    l->m_win = SDL_CreateWindowFrom((void *)l->m_labelBody->winId());
    if (!l->m_win) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create window by SDL");
        return;
    }

    l->m_renderer = SDL_CreateRenderer(l->m_win, -1, 0);
    if (!l->m_renderer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create Renderer by SDL");
        return;
    }

    Uint32 pixformat = SDL_PIXELFORMAT_IYUV;
    l->m_texture = SDL_CreateTexture(l->m_renderer, pixformat, SDL_TEXTUREACCESS_STREAMING, w_width, w_height);
}

//单机某个摄像头
void MainWindow::startOneSDL()
{
    {
        std::lock_guard<std::mutex> mylockguard(m_mutex);
        QToolButton *button = qobject_cast<QToolButton*>(sender());
        QString buttonText = button->text();

        //不存在
        if(mapSSRC.find(buttonText) == mapSSRC.end()){
            int currentCount = mapSSRC.size();
            SdlLabel* l = new SdlLabel(buttonText, this);
            QGridLayout* gridLayout = this->findChild<QGridLayout*>("gridLayout");
            l->SDLWin(gridLayout, currentCount);
            mapSSRC.emplace(buttonText, l);

            //绑定信号 开始按钮 视频窗口的双击和esc退出
            connect(l->m_btnStart, &QToolButton::clicked, this, &MainWindow::startThreadClicked);

        }else{
            //这种可以弹框
            QMessageBox::information(this, "!", QString("已在列表中！").arg(buttonText));
        }
    }
}

//这个是开始播放按钮槽
void MainWindow::startThreadClicked()
{
    QToolButton *button = qobject_cast<QToolButton*>(sender());
    QVariant isflat = button->property("name");
    QString ssrc = isflat.toString();
    SdlLabel* sdlLabel = mapSSRC[ssrc];

    //可以开始了，待处理：这里需要做一个是否开启成功 然后才改变按钮，当然所有的按钮都还要做防止连续点击
    if(sdlLabel->status == SdlLabel::Status::nonactive){
        sdlLabel->changeStatus(SdlLabel::Status::active);
        button->setText("停止");

        //播放线程
        qDebug()<<"开启解码线程";
        std::thread start1(&MainWindow::startMain, this, sdlLabel, this);
        start1.detach();

    }else if(sdlLabel->status == SdlLabel::Status::active){
        sdlLabel->changeStatus(SdlLabel::Status::nonactive);
        button->setText("开始");
        //QMessageBox::information(this, "!", QString("正在播放中！").arg(ssrc));
    }else if(sdlLabel->status == SdlLabel::Status::stopped){

    }
}

void MainWindow::startMain(SdlLabel* sdlLabel, MainWindow* w)
{
    std::string url = ssrc_ip + sdlLabel->m_name.toStdString();
    //声明所需的变量名
    int ret = 0;
    //set defualt size of window
    int w_width = 640, w_height = 480;

    int videoIndex = -1, audioIndex = -1;
    float videoTimeBase = 0, audioTimeBase = 0;

    //----------------- 创建AVFormatContext结构体：总体的I/O内容 -------------------
    //内部存放着描述媒体文件或媒体流的构成和基本信息
    AVFormatContext* fmtCtx = avformat_alloc_context();
    AVCodecContext* codecCtxVideo = NULL;
    AVCodecContext* codecCtxAudio = NULL;
    AVCodecParameters* avCodecParaVideo = NULL;
    AVCodecParameters* avCodecParaAudio = NULL;

    const AVCodec* codecVideo = nullptr;
    const AVCodec* codecAudio = nullptr;

    //----------------- 打开输入流并且读相应的头信息 -------------------
    ret = avformat_open_input(&fmtCtx, url.c_str(), NULL, NULL);
    if (ret) {
        //待处理：打不开流时要发信号给主线程，弹框提示以及改变按钮文字为“开始”
        qDebug()<<"cannot open stream";
        goto __FAIL;
    }

    //----------------- 获取多媒体文件音视频流信息 -------------------
    ret = avformat_find_stream_info(fmtCtx, NULL);
    if (ret < 0) {
        printf("Cannot find stream information\n");
        goto __FAIL;
    }

    //通过循环查找多媒体文件中包含的流信息，直到找到视频类型的流，并记录该索引值
    for (int i = 0; i < fmtCtx->nb_streams; i++) {
        //从文件的每个流的编码流参数中的编码数据：可能是音频、视频、副标题等等，这里要找一段流信息中第几个流是视频流
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoIndex = i;
            videoTimeBase = av_q2d(fmtCtx->streams[i]->time_base);
            //m_image_device->setframeRate(fmtCtx->streams[i]->avg_frame_rate.num);   //视频帧率
        }
        //这里是找音频流的索引
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioIndex = i;
            audioTimeBase = av_q2d(fmtCtx->streams[i]->time_base);
        }
    }

    //没找到视频流
    if (videoIndex == -1 || audioIndex == -1) {
        printf("cannot find video stream\n");
        goto __FAIL;
    }

    //打印流信息
    //Print detailed information about the input or output format, such as duration, bitrate,
    //streams, container, programs, metadata, side data, codec and time base.
    av_dump_format(fmtCtx, 0, url.c_str(), 0);

    //----------------- 查找视频解码器：这就是该视频流中所用到的解码器 -------------------
    avCodecParaVideo = fmtCtx->streams[videoIndex]->codecpar;    //解码结构体
    codecVideo = avcodec_find_decoder(avCodecParaVideo->codec_id); //codec_id：Specific type of the encoded data//然后通过id找到编解码信息
    if (codecVideo == NULL) {
        printf("cannot open decoder codecVideo\n");
        goto __FAIL;
    }

    //查找音频解码器
    if (audioIndex != -1) {
        avCodecParaAudio = fmtCtx->streams[audioIndex]->codecpar;    //解码结构体
        codecAudio = avcodec_find_decoder(avCodecParaAudio->codec_id);
        if (codecAudio == NULL) {
            printf("cannot open decoder codecAudio\n");
            goto __FAIL;
        }
    }

    //根据解码器参数来创建解码器上下文
    /*
     *  生成AVCodecContext，这个结构体里有音频的有视频的，比如
        width/height  视频的宽与高
        codec_id 编解码器的id，根据它可以找到对应的编解码器
        extradata 对于h264编码的视频，保存了pps与sps的参数信息
        sample_rate 音频的采样率
        channels 音频的声道数
        sample_fmt 音频的采样格式
        profile 视频编码复杂等级
        level：级（和profile差不太多）
    */
    codecCtxVideo = avcodec_alloc_context3(codecVideo);   //AVCodecContext初始化
    //从原AVStream中给AVCodecContext添加数据
    ret = avcodec_parameters_to_context(codecCtxVideo, avCodecParaVideo);
    if (ret < 0) {
        printf("parameters to context fail codecCtxVideo\n");
        goto __FAIL;
    }
    if (audioIndex != -1) {
        codecCtxAudio = avcodec_alloc_context3(codecAudio);
        ret = avcodec_parameters_to_context(codecCtxAudio, avCodecParaAudio);
        if (ret < 0) {
            printf("parameters to context fail codecAudio\n");
            goto __FAIL;
        }
    }

    //----------------- 打开解码器 -------------------
    //这个函数完成3个动作：1.参数检查与设置；2.初始化codec线程；3.初始化codec。
    ret = avcodec_open2(codecCtxVideo, codecVideo, NULL);
    if (ret < 0) {
        printf("cannot open initial codec thread codecCtxVideo\n");
        goto __FAIL;
    }
    if (audioIndex != -1) {
        ret = avcodec_open2(codecCtxAudio, codecAudio, NULL);
        if (ret < 0) {
            printf("cannot open initial codec thread codecCtxAudio\n");
            goto __FAIL;
        }
    }

    w_width = codecCtxVideo->width;
    w_height = codecCtxVideo->height;

    emit w->initialSDLwin_s(sdlLabel, w_width, w_height);

    {
        //这里decodeThread线程出来后 也就是流关闭后这个参数会变成随机值，如果线程执行完析构会报异常，但是线程前执行的话播放就会死掉了
        //※：？这里难道要传进去析构
        //        if (avCodecParaVideo) {
        //            avcodec_parameters_free(&avCodecParaVideo);
        //        }

        //        qDebug()<<"delete avCodecParaVideo";

        //        if (avCodecParaAudio) {
        //            avcodec_parameters_free(&avCodecParaAudio);
        //        }

        //        qDebug()<<"delete avCodecParaAudio";

        std::thread d(&MainWindow::decodeThread, w, fmtCtx, videoIndex, audioIndex, codecCtxVideo, codecCtxAudio, videoTimeBase,
                      audioTimeBase, w_width, w_height, sdlLabel);
        d.join();
    }

__FAIL:

    sdlLabel->changeStatus(SdlLabel::Status::nonactive);

    qDebug()<<"startMain delete start:";

    if (sdlLabel->m_win) {
        SDL_DestroyWindow(sdlLabel->m_win);
    }
    qDebug()<<"delete SDL_DestroyWindow";

    if (sdlLabel->m_renderer) {
        SDL_DestroyRenderer(sdlLabel->m_renderer);
    }
    qDebug()<<"delete SDL_DestroyRenderer";

    if (sdlLabel->m_texture) {
        SDL_DestroyTexture(sdlLabel->m_texture);
    }
    qDebug()<<"delete SDL_DestroyTexture";

    if (fmtCtx) {
        avformat_close_input(&fmtCtx);
    }
    qDebug()<<"delete fmtCtx";

    // Close the codec
    if (codecCtxVideo) {
        avcodec_free_context(&codecCtxVideo);
    }
    qDebug()<<"delete codecCtxVideo";

    if (codecCtxAudio) {
        avcodec_free_context(&codecCtxAudio);
    }
    qDebug()<<"delete codecCtxAudio";

    //待处理：这里图片类和声音类析构还有点问题
    // if (sdlLabel->m_image) {
    //     //播放线程完毕才能释放
    //     while(sdlLabel->threadImage)
    //     {
    //         std::this_thread::sleep_for(std::chrono::milliseconds(10));
    //     }
    //     delete sdlLabel->m_image;
    // }
    // qDebug()<<"delete m_image";

    // if (sdlLabel->m_voice) {
    //     //播放线程完毕才能释放
    //     while(sdlLabel->threadVoice)
    //     {
    //         std::this_thread::sleep_for(std::chrono::milliseconds(10));
    //     }
    //     delete sdlLabel->m_voice;
    // }
    // qDebug()<<"delete m_voice";
}

//解码线程
void MainWindow::decodeThread(AVFormatContext* fmtCtx, int videoIndex, int audioIndex, AVCodecContext* codecCtxVideo,
                              AVCodecContext* codecCtxAudio, float videoTimeBase, float audioTimeBase, int w_width, int w_height,
                              SdlLabel* sdlLabel)
{
    static int delayNumber = 1000;
    std::this_thread::sleep_for(std::chrono::milliseconds(delayNumber));
    delayNumber += 1000;

    static int threadNumber = 1;
    int currentThread = threadNumber++;
    int ret = 0;
    int audioDeviceInitial = 0;

    // 初始化音频需要的SwrContext
    SwrContext* sws_ctx_ForAudio = swr_alloc(); //音频重采样转换结构体

    //音频相关参数，重采样用到
    int initial_sample_rate, initial_nb_chnnels; AVSampleFormat initial_coded_sample;
    if (audioIndex != -1) {
        initial_sample_rate = codecCtxAudio->sample_rate;           //输出采样率
        initial_nb_chnnels = codecCtxAudio->ch_layout.nb_channels;  //输出通道数
        initial_coded_sample = AV_SAMPLE_FMT_S16;                   //输出采样位

        AVChannelLayout outChannelLayout = codecCtxAudio->ch_layout;
        outChannelLayout.nb_channels = initial_nb_chnnels;
        AVChannelLayout inChannelLayout = codecCtxAudio->ch_layout;
        inChannelLayout.nb_channels = codecCtxAudio->ch_layout.nb_channels;
        int ret = swr_alloc_set_opts2(&sws_ctx_ForAudio, &outChannelLayout, initial_coded_sample, initial_sample_rate,
                                      &inChannelLayout, codecCtxAudio->sample_fmt, codecCtxAudio->sample_rate, 0, NULL);

        if (ret < 0) {
            printf("swr_alloc_set_opts2 error\n");
            return ;
        }
        swr_init(sws_ctx_ForAudio);
    }
    else {
        initial_sample_rate = 44100;                                //输出采样率
        initial_nb_chnnels = 2;                                     //输出通道数
        initial_coded_sample = AV_SAMPLE_FMT_S16;                   //输出采样位
    }

    //----------------- 创建AVPacket和AVFrame结构体 -------------------
    AVPacket* pkt = av_packet_alloc();      //压缩数据包
    AVFrame* frame = av_frame_alloc();      //音视频帧
    uint8_t* outputBuffer = nullptr;        //音频数据空间
    long long timestamp;

    while (av_read_frame(fmtCtx, pkt) >= 0) {

        if(sdlLabel->status == SdlLabel::Status::nonactive){
            emit sdlLabel->SclearWin();
            break;
        }

        if (pkt->stream_index == videoIndex) {
            ret = avcodec_send_packet(codecCtxVideo, pkt);
            if (ret == 0) {
                while (avcodec_receive_frame(codecCtxVideo, frame) == 0) {


                    timestamp = frame->best_effort_timestamp * videoTimeBase * 1000;
                    if (audioDeviceInitial == 1) {

                        //视频类初始化
                        sdlLabel->m_image = new ImageInfo(sdlLabel->m_renderer, sdlLabel->m_texture, w_width, w_height, sdlLabel->m_voice,
                                                             frame->linesize[0], frame->linesize[1], frame->linesize[2], MAXVOICEVECTOR / 2);

                        //视频播放线程
                        qDebug()<<"视频播放线程开启";
                        std::thread p1(&MainWindow::playVideo, this, sdlLabel, sdlLabel->m_image);
                        p1.detach();

                        cout << "videoDeviceInitial:"<< currentThread << endl;
                        audioDeviceInitial = 2;
                    }

                    if (timestamp > 0 && sdlLabel->m_image) {
                        sdlLabel->m_image->push(frame->data[0], frame->linesize[0], frame->data[1], frame->linesize[1],
                                                          frame->data[2], frame->linesize[2], timestamp);
                    }

                }
                av_frame_unref(frame);
            }
        }
        if (pkt->stream_index == audioIndex) {
            ret = avcodec_send_packet(codecCtxAudio, pkt);
            if (ret == 0) {
                while (avcodec_receive_frame(codecCtxAudio, frame) == 0) {

                    int out_buffer_size = av_samples_get_buffer_size(NULL, initial_nb_chnnels, frame->nb_samples, initial_coded_sample, 0);

                    //首帧音频需要干两件事：第一是初始化音频设备；第二是为每帧音频数据分配空间

                    if (audioDeviceInitial == 0) {
                        //音频类
                        sdlLabel->m_voice = new VoiceInfo(MAXVOICEVECTOR, out_buffer_size);

                        //音频播放线程
                        qDebug()<<"音频播放线程开启";
                        std::thread p2(&MainWindow::playAudio, this, sdlLabel, sdlLabel->m_voice);
                        p2.detach();

                        audioDeviceInitial = 1;

                        outputBuffer = (uint8_t*)av_malloc(out_buffer_size);

                        cout << "audioDeviceInitial:"<< currentThread << endl;
                    }

                    //重采样返回重采样后的样本数量
                    int len = swr_convert(sws_ctx_ForAudio, &outputBuffer, out_buffer_size,
                                          (const uint8_t**)frame->data, frame->nb_samples);
                    if (len <= 0) {
                        continue;
                    }

                    //偶尔会出现负很多的值 不知道什么原因
                    timestamp = frame->best_effort_timestamp * audioTimeBase * 1000;
                    if(timestamp > 0){
                        sdlLabel->m_voice->push(outputBuffer, out_buffer_size, timestamp);
                    }

                }
                av_frame_unref(frame);
            }
        }

        av_packet_unref(pkt);
        //av_packet_free(&pkt);
        //pkt = av_packet_alloc();
    }

    qDebug()<<"decodeThread delete start:";

    if (sws_ctx_ForAudio) {
        swr_free(&sws_ctx_ForAudio);
    }

    qDebug()<<"delete sws_ctx_ForAudio";

    if (pkt) {
        av_packet_free(&pkt);
    }

    qDebug()<<"delete pkt";

    if (frame) {
        av_frame_free(&frame);
    }

    qDebug()<<"delete frame";

    if (outputBuffer) {
        av_free(outputBuffer);
    }

    qDebug()<<"delete outputBuffer";
}

//这里要做一个线程开始和结束的标志，用于后续delete的判断，只有play线程结束后才能delete
void MainWindow::playVideo(SdlLabel* sdlLabel, ImageInfo* image)
{
    sdlLabel->threadImage = true;
    image->play();
    sdlLabel->threadImage = false;
}

void MainWindow::playAudio(SdlLabel* sdlLabel, VoiceInfo* voice)
{
    sdlLabel->threadImage = true;
    voice->play();
    sdlLabel->threadImage = false;
}

//刷新按钮槽
void MainWindow::on_toolButton_refresh_clicked()
{

    std::map<int, std::string> responseMap = curlHttp("refresh");
    std::string response = responseMap.begin()->second;
    if(responseMap.begin()->first != 200){
        QMessageBox::information(this, "!", QString::fromStdString(response));
        return;
    }

    // 解析 JSON 字符串
    QString jsonString = QString::fromUtf8(response.c_str());
    QJsonDocument jsonDocument = QJsonDocument::fromJson(jsonString.toUtf8());

    if (!jsonDocument.isNull()) {
        if (jsonDocument.isObject()) {
            QJsonObject jsonObject = jsonDocument.object();

            int code = jsonObject["code"].toInt();
            qDebug() << "code:" << code;
            if(code != 200){
                QMessageBox::information(this, "!", QString::fromStdString("未知错误"));
                return;
            }

            QVBoxLayout* verticalLayout = this->findChild<QVBoxLayout*>("verticalLayout");

            /* 查找 QVBoxLayout 中的所有 QToolButton和Stretch并删除
                     * Stretch 和 Spacing 都属于 QSpacerItem
                     * 使用removeItem 删除：
                    */
            int itemCount = verticalLayout->count();
            std::vector<QToolButton*> deleteList;
            for (int i = 0; i < itemCount; ++i) {
                QLayoutItem *item = verticalLayout->itemAt(i);
                if (QWidget *widget = item->widget()) {
                    if (QToolButton *temp_button = qobject_cast<QToolButton*>(widget)) {
                        // 找到 QPushButton，进行相应操作
                        // 在这个示例中，我们简单地输出按钮的文本
                        QString buttonText = temp_button->text();
                        qDebug() << "Found QToolButton: " << buttonText;
                        deleteList.emplace_back(temp_button);
                    }
                }
            }
            for(auto i : deleteList){
                verticalLayout->removeWidget(i);
                delete i;
            }
            for (int i = 0; i < verticalLayout->count(); ++i) {
                QLayoutItem *layoutItem = verticalLayout->itemAt(i);
                if(layoutItem->spacerItem())
                {
                    verticalLayout->removeItem(layoutItem);
                    i--;
                }
            }

            // 获取 activeList 数组
            QJsonArray activeListArray = jsonObject["activeList"].toArray();
            std::vector<QString> activeListVetor;
            for (const QJsonValue& ageValue : activeListArray) {
                QString age = ageValue.toString();
                activeListVetor.emplace_back(age);
            }

            //生成当前摄像头列表
            QJsonArray totalListArray = jsonObject["totalList"].toArray();
            int totalListNum = totalListArray.count();
            // 遍历数组元素
            for (const QJsonValue& ageValue : totalListArray) {
                QString age = ageValue.toString();
                QToolButton* tbtn = new QToolButton();
                tbtn->setText(age);
                if(std::find(activeListVetor.begin(), activeListVetor.end(), age) != activeListVetor.end()){
                    tbtn->setStyleSheet("QToolButton { border: none;}");
                    verticalLayout->insertWidget(1, tbtn, 0, Qt::AlignLeft);
                    //这里想要双击 必须重写QToolButton
                    connect(tbtn, &QToolButton::clicked, this, &MainWindow::startOneSDL);
                }else{
                    tbtn->setStyleSheet("QToolButton { border: none; color: gray;}");
                    verticalLayout->addWidget(tbtn, 0, Qt::AlignLeft);
                }
            }
            verticalLayout->addStretch(9 - totalListNum);
        }
    }
}

void MainWindow::closeMKF()
{
    mkfStatus = mkfEnum::noactive;
    mkfPid = 0;
    mkfStreamName = "";
    QLabel* labelMFKStatus = this->findChild<QLabel*>("labelMFKStatus");
    labelMFKStatus->setText("");
    QPushButton *button = this->findChild<QPushButton*>("pushButtonMKF");
    button->setText("打开麦克风");
    timer->stop();

    QJsonArray jsonArray;
    for(auto iter = mapSSRC.begin(); iter != mapSSRC.end(); ++iter){
        SdlLabel * s = iter->second;
        if(s->isTalk == 1){
            emit s->ScloseTalk();
            jsonArray.append(iter->first);
        }
    }
    qDebug()<<"麦克风已关闭：";

    // 创建 JSON 对象并将 JSON 数组添加为一个属性
    QJsonObject jsonObject;
    jsonObject["ssrcList"] = jsonArray;
    jsonObject["type"] = 1;

    // 创建 JSON 文档
    QJsonDocument jsonDoc(jsonObject);
    QByteArray jsonData = jsonDoc.toJson();

    //发送http 告诉sip服务器这边已经关闭对讲了
    std::map<int, std::string> responseMap = curlHttp("stopTalk", QString::fromUtf8(jsonData));
    std::string response = responseMap.begin()->second;
    qDebug()<<responseMap.begin()->second;
}

void MainWindow::mkfProcessDetect()
{
    bool isExsit = isExistProcess(mkfPid);
    if(isExsit){
        qDebug()<<"检测到麦克风正在传输："<<mkfPid;
    }else{
        qDebug()<<"检测到麦克风关闭：";
        this->closeMKF();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QMessageBox *msgBox = new QMessageBox(QMessageBox::Question, tr("退出"), tr("真的退出吗？"), QMessageBox::Yes | QMessageBox::No);

    // 将原本显示“Yes”的按钮改为显示“是”
    msgBox->button(QMessageBox::Yes)->setText("是");

    // 将原本显示“No”的按钮改为显示“否”
    msgBox->button(QMessageBox::No)->setText("否");

    // 启动对话框，用res变量记录用户最终点选的按钮
    int res = msgBox->exec();
    if(QMessageBox::Yes == res){
        event->accept();
        //关闭麦克风进程 提醒服务器重置推流状态
        if(mkfStatus == mkfEnum::active){
            TerminateProcessTree(mkfPid);
            this->closeMKF();
            qDebug()<<"关闭麦克风进程 提醒服务器重置推流状态";
        }
    }
    else if(QMessageBox::No == res){
        event->ignore();
    }
}

//麦克风推流
void MainWindow::on_pushButtonMKF_clicked()
{
    if(mkfStatus == mkfEnum::active){
        TerminateProcessTree(mkfPid);
        this->closeMKF();
        return;
    }

    std::wstring macOUT = getWindowMacAddress();
    QString streamName = stdWToQString(macOUT);
    QString microphoneName = getAudioDeviceName();
    qDebug()<<"麦克风名称："<<microphoneName;
    if(my_CreateProcess(lpExecInfo, microphoneName, streamName)){

        int maxTime = 25;
        int startTime = 0;
        DWORD processId = GetProcessId(lpExecInfo.hProcess);
        while(!processId && startTime < maxTime){
            std::this_thread::sleep_for(std::chrono::seconds(1));
            ++startTime;
            processId = GetProcessId(lpExecInfo.hProcess);
        }

        if(!processId){
            QMessageBox::information(this, "!", "麦克风打开失败");
        }else{
            mkfStatus = mkfEnum::active;
            mkfPid = processId;
            mkfStreamName = streamName;
            QLabel* labelMFKStatus = this->findChild<QLabel*>("labelMFKStatus");
            labelMFKStatus->setText("麦克风已打开");
            labelMFKStatus->setStyleSheet("color:green;");
            QPushButton *button = qobject_cast<QPushButton*>(sender());
            button->setText("关闭麦克风");
            timer->start();
        }
    }
}

