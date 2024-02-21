#include "login.h"
#include "ui_login.h"

#include "utils.h"
#include <curl/curl.h>
#include<QMessageBox>

Login::Login(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Login)
{
    ui->setupUi(this);
    w = new MainWindow();

    this->findChild<QLineEdit*>("lineEdit_password")->setEchoMode(QLineEdit::Password);
}

Login::~Login()
{
    delete ui;
    delete w;
}

//登录
void Login::on_pushButton_login_clicked()
{
    QString account = this->findChild<QLineEdit*>("lineEdit_account")->text();
    QString password = this->findChild<QLineEdit*>("lineEdit_password")->text();

    if(account.isEmpty() || password.isEmpty()){
        QLabel * l = this->findChild<QLabel*>("label_reminbe");
        l->setText("请输入账号密码");
        l->setStyleSheet("color:blue;");
        return;
    }

    // HttpClient("http://localhost:8080/device").get([](const QString &response) {
    // qDebug() << response;


    QString qdata = QString("{\"account\":\"") + account + QString("\",\"password\":\"") + password + QString("\"}");
    std::map<int, std::string> responseMap = curlHttp("login", qdata);
    std::string response = responseMap.begin()->second;
    if(responseMap.begin()->first != 200){
        QMessageBox::information(this, "!", QString::fromStdString(response));
        return;
    }

    std::map<std::string, std::string> jsonInfo = getJsonInfo(response.c_str(), response.length());
    QString code;
    if(jsonInfo.find("code") != jsonInfo.end()){
        code = QString::fromStdString(jsonInfo[std::string("code")]);
    }else{
        QLabel * l = this->findChild<QLabel*>("label_reminbe");
        l->setText("未知错误");
        l->setStyleSheet("color:red;");
        return;
    }

    if(code == "200"){
        qDebug()<<QString::fromStdString(jsonInfo[std::string("msg")]);
        this->hide();
        w->show();
    }else{
        if(jsonInfo.find("msg") != jsonInfo.end()){
            QString msg = QString::fromStdString(jsonInfo[std::string("msg")]);
            QLabel * l = this->findChild<QLabel*>("label_reminbe");
            l->setText(msg);
            l->setStyleSheet("color:red;");
            qDebug()<<msg;
        }
    }

}


void Login::on_lineEdit_account_selectionChanged()
{
    QLabel * l = this->findChild<QLabel*>("label_reminbe");
    l->clear();
}


void Login::on_lineEdit_account_textEdited(const QString &arg1)
{
    QLabel * l = this->findChild<QLabel*>("label_reminbe");
    l->clear();
}


void Login::on_lineEdit_password_textChanged(const QString &arg1)
{
    QLabel * l = this->findChild<QLabel*>("label_reminbe");
    l->clear();
}

