#ifndef LOGIN_H
#define LOGIN_H

#include "mainwindow.h"

#include <QWidget>

namespace Ui {
class Login;
}

class Login : public QWidget
{
    Q_OBJECT

public:
    explicit Login(QWidget *parent = nullptr);
    ~Login();

private slots:
    void on_pushButton_login_clicked();

    void on_lineEdit_account_selectionChanged();

    void on_lineEdit_account_textEdited(const QString &arg1);

    void on_lineEdit_password_textChanged(const QString &arg1);

private:
    Ui::Login *ui;
    MainWindow *w;
};

#endif // LOGIN_H
