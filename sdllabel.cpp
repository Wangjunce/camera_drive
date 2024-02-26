#include "sdllabel.h"
#include "mainwindow.h"
#include <QToolButton>

#include "utils.h"
#include <curl/curl.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>

SdlLabel::SdlLabel(QString buttonText, QWidget *parent)  : QLabel(parent) {
    m_name = buttonText;
    status = Status::nonactive;
    isTalk = 0;
    threadImage = false;

    connect(this, &SdlLabel::SchangeSDL, this, &SdlLabel::changeSDL);
    connect(this, &SdlLabel::SclearWin, this, &SdlLabel::clearWin);
    connect(this, &SdlLabel::ScloseTalk, this, &SdlLabel::closeTalk);
}

SdlLabel::~SdlLabel(){

}

void SdlLabel::changeStatus(Status s){
    status = s;
}

void SdlLabel::SDLWin(QGridLayout* gridLayout, int currentCount){

    m_title = new QLabel();
    m_title->setText(m_name);
    m_title->setStyleSheet("background-color: grey; color: white;");
    m_title->setAlignment(Qt::AlignCenter);
    m_labelBody = new MyLabel(this);
    m_labelBody->setStyleSheet("background-color: black; color: white;");
    m_btnStart = new QToolButton();
    m_btnStart->setText("开始");
    m_btnStart->setProperty("name", m_name);
    m_btnTalk = new QToolButton();
    m_btnTalk->setText("开始对讲");
    m_btnVoice = new QToolButton();
    m_btnVoice->setText("静音");

    gridLayout->addWidget(m_title, (int)(currentCount / 3) * 6, currentCount % 3 * 5 + 1, 1, 3);
    gridLayout->addWidget(m_labelBody, (int)(currentCount / 3) * 6 + 1, currentCount % 3 * 5, 5, 5);
    gridLayout->addWidget(m_btnStart, (int)(currentCount / 3) * 6 + 5, currentCount % 3 * 5, 1, 1);
    gridLayout->addWidget(m_btnTalk, (int)(currentCount / 3) * 6 + 5, currentCount % 3 * 5 + 2, 1, 1);
    gridLayout->addWidget(m_btnVoice, (int)(currentCount / 3) * 6 + 5, currentCount % 3 * 5 + 4, 1, 1);

    connect(m_btnTalk, &QToolButton::clicked, this, &SdlLabel::talkInfo);
    connect(m_btnVoice, &QToolButton::clicked, this, &SdlLabel::changeVoice);
}

void SdlLabel::clearWin(){
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_renderer);
    SDL_RenderPresent(m_renderer);
}

void SdlLabel::talkInfo(){
    QToolButton *button = qobject_cast<QToolButton*>(sender());
    QString text = button->text();
    if(text == "开始对讲"){
        if(isTalk == 1){
            QMessageBox::information(this, "!", QString::fromStdString("不可重复对讲"));
            return;
        }

        if(!threadImage){
            QMessageBox::information(this, "!", QString::fromStdString("请先打开摄像头"));
            return;
        }
        talkClicked();
    }else{
        if(isTalk == 1){
            MainWindow* w = qobject_cast<MainWindow*>(this->parent());
            TerminateProcessTree(w->getMkfPid());
            w->closeMKF();
        }else{
            button->setText("开始对讲");
        }
    }
}

void SdlLabel::talkClicked(){

    MainWindow* w = qobject_cast<MainWindow*>(this->parent());
    QString streamName = w->getMkfStreamName();
    if(streamName == ""){
        QMessageBox::information(this, "!", QString::fromStdString("请先打开麦克风"));
        return;
    }
    QString ssrc = m_name;

    QString sendInfo = "{\"ssrc\":\""+m_name+"\", \"streamName\":\""+streamName+"\"}";
    std::map<int, std::string> responseMap = curlHttp("talk", sendInfo);
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

            m_btnTalk->setText("停止对讲");
            isTalk = 1;
        }
    }
}

void SdlLabel::hideAllChildren(){
    if(status == Status::active){
        status = Status::stopped;
    }

    m_title->hide();
    m_labelBody->hide();
    m_btnStart->hide();
    m_btnTalk->hide();
    m_btnVoice->hide();
}

void SdlLabel::showAllChildren(){
    m_title->show();
    m_labelBody->show();
    m_btnStart->show();
    m_btnTalk->show();
    m_btnVoice->show();
    if(status == Status::stopped){
        status = Status::active;
    }
}

void SdlLabel::closeTalk(){

    m_btnTalk->setText("开始对讲");
    isTalk = 0;
}

void SdlLabel::changeVoice(){
    QToolButton *button = qobject_cast<QToolButton*>(sender());
    QString text = button->text();
    if(text == "静音"){
        m_voice->setVolume(false);
        button->setText("开音");
    }else{
        m_voice->setVolume(true);
        button->setText("静音");
    }
}

//因为图像长宽改变 重新初始化sdl相关部件
void SdlLabel::changeSDL(){
    status = Status::stopped;
    SDL_DestroyWindow(m_win);
    SDL_DestroyRenderer(m_renderer);
    SDL_DestroyTexture(m_texture);

    MainWindow* w = qobject_cast<MainWindow*>(this->parent());
    m_win = SDL_CreateWindowFrom((void *)m_labelBody->winId());
    if (!m_win) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create window by SDL");
        return;
    }

    m_renderer = SDL_CreateRenderer(m_win, -1, 0);
    if (!m_renderer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create Renderer by SDL");
        return;
    }

    Uint32 pixformat = SDL_PIXELFORMAT_IYUV;
    m_texture = SDL_CreateTexture(m_renderer, pixformat, SDL_TEXTUREACCESS_STREAMING, w->width(), w->height());
    m_image->changeSDL(m_renderer, m_texture);
    status = Status::active;
}
