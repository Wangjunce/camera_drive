#include "mylabel.h"
#include "sdllabel.h"
#include "mainwindow.h"

MyLabel::MyLabel(SdlLabel* sdlLabel, QWidget *parent) : QLabel(parent) {
    m_sdlLabel = sdlLabel;
    //qDebug()<<"m_labelBody4: "<<this->width()<<"-"<<this->height();

    flag = 0;
}

MyLabel::~MyLabel()
{
    m_sdlLabel = nullptr;
}

void MyLabel::mouseDoubleClickEvent(QMouseEvent* event)
{
    qDebug()<<"mouseDoubleClickEvent:"<<this->width()<<" "<<this->height();

    if(flag == 0){
        originalWidth = this->width();
        originalHeight = this->height();
        QWidget* parentWindow = qobject_cast<QWidget*>(this->parent());
        qDebug()<<"parentWindow:"<<parentWindow->width()<<" "<<parentWindow->height();
        this->setFixedWidth(parentWindow->width());
        this->setFixedHeight(parentWindow->height());
        qDebug()<<parentWindow->objectName();
        flag = 1;
        if(m_sdlLabel->status == SdlLabel::Status::active){
            qDebug()<<"change:"<<this->width()<<" "<<this->height();
            emit m_sdlLabel->SchangeSDL();
            qDebug()<< "摄像头的ssrc1:" << m_sdlLabel->m_name;
        }

        //m_sdlLabel->hide();

        // MainWindow* w = qobject_cast<MainWindow*>(m_sdlLabel->parent());
        // std::map<QString, SdlLabel*> sList = w->getSdlLabelList();
        // for(auto it = sList.begin(); it != sList.end(); ++it){
        //     qDebug()<< "it->second->m_name:" << it->second->m_name;
        //     if(it->second->m_name != m_sdlLabel->m_name){
        //         delete it->second;
        //     }
        // }
    }else{
        this->setFixedWidth(originalWidth);
        this->setFixedHeight(originalHeight);
        flag = 0;
        if(m_sdlLabel->status == SdlLabel::Status::active){
            qDebug()<<"change:"<<this->width()<<" "<<this->height();
            emit m_sdlLabel->SchangeSDL();
            qDebug()<< "摄像头的ssrc2:" <<m_sdlLabel->m_name;
        }

        MainWindow* w = qobject_cast<MainWindow*>(m_sdlLabel->parent());
        std::map<QString, SdlLabel*> sList = w->getSdlLabelList();
        for(auto it = sList.begin(); it != sList.end(); ++it){
            if(it->second->m_name != m_sdlLabel->m_name){
                it->second->show();
            }
        }
    }
}

void MyLabel::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        // 按下 Esc 键时，退出全屏模式
        setWindowFlags(Qt::SubWindow);
        this->showNormal();
        if(m_sdlLabel->status == SdlLabel::Status::active){
            emit m_sdlLabel->SchangeSDL();
        }
    }
}

void MyLabel::slotsClicked()
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}
