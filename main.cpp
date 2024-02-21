//main.cpp中不添加这行会报错：SDL2main.lib(SDL_windows_main.obj):-1: error: LNK2005: main 已经在 main.cpp.obj 中定义
#pragma comment( linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"" )

#include "mainwindow.h"
#include "login.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Login l;
    l.show();
    //MainWindow w;
    //w.show();


    return a.exec();
}
