#ifndef MYLABEL_H
#define MYLABEL_H

#include <QLabel>
#include <QObject>
#include <QMouseEvent>

class SdlLabel;

class MyLabel : public QLabel
{
    Q_OBJECT
public:
    explicit MyLabel(SdlLabel* sdlLabel, QWidget *parent = nullptr);
    ~MyLabel();
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
signals:

public slots:
    void slotsClicked();
private:
    SdlLabel* m_sdlLabel;
    bool flag;
    int originalWidth;
    int originalHeight;
};

#endif // MYLABEL_H
