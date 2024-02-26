#ifndef SDLLABEL_H
#define SDLLABEL_H

#include <QLabel>
#include <QObject>
#include <QGridLayout>

#include <Windows.h>
#include <SDL.h>

#include "imageinfo.h"
#include "voiceinfo.h"
#include "mylabel.h"

class SdlLabel : public QLabel
{
    Q_OBJECT
public:
    enum class Status
    {
        active = 1,
        nonactive = 2,
        stopped = 3
    };
    explicit SdlLabel(QString buttonText, QWidget *parent = nullptr);
    ~SdlLabel();
    //改变状态
    void changeStatus(Status s);
    //初始化按钮和位置
    void SDLWin(QGridLayout* gridLayout, int currentCount);

    void hideAllChildren();
    void showAllChildren();

    Status status;          //状态
    QString m_name;         //摄像头ssrc
    SDL_Window *m_win;
    SDL_Renderer *m_renderer;
    SDL_Texture *m_texture;

    QLabel *m_title;
    MyLabel *m_labelBody;
    QToolButton *m_btnStart;
    QToolButton *m_btnTalk;
    QToolButton *m_btnVoice;
    ImageInfo *m_image;
    VoiceInfo *m_voice;
    bool threadImage;
    int isTalk;

signals:
    void SclearWin();
    void ScloseTalk();
    void SchangeSDL();
    void SchangeVoice();

public slots:
    void clearWin();
    void changeSDL();
    void talkInfo();
    void closeTalk();
    void talkClicked();
    void changeVoice();
};

#endif // SDLLABEL_H
