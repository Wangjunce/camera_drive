#ifndef UTILS_H
#define UTILS_H

#include <map>
#include<string>
#include <curl/curl.h>
#include "sdllabel.h"

// #define SIPHTTPURL "192.168.5.103:8006"         //sip服务器的http端口
// #define MEDIATALKURL "192.168.5.103:1935"       //媒体服务器的麦克风推流端口
// #define RTPURL "rtsp://192.168.5.103/rtp/"      //媒体服务器推流接口

#define SIPHTTPURL "110.42.208.206:8006"         //sip服务器的http端口
#define MEDIATALKURL "110.42.208.206:1935"       //媒体服务器的麦克风推流端口
#define RTPURL "rtsp://110.42.208.206/rtp/"      //媒体服务器推流接口

//解析json, 自己写的，能解base64图片 但是不能解数组
std::map<std::string, std::string> getJsonInfo(const char* buff_data, int data_len);
//curl的回调
size_t req_reply(void* ptr, size_t size, size_t nmemb, void* stream);
//根据一个label和已知的长宽生成相关的sdl对象实例
void initialSDLwin(SdlLabel* l, int w_width, int w_height);

//获取麦克风名称
QString getAudioDeviceName();

wchar_t* QString2Wchar(QString buf);
QString Wchar2QString(wchar_t *buf);
//创建进程
int my_CreateProcess(SHELLEXECUTEINFO& lpExecInfo, const QString& microphoneName, const QString& streamName);

//这两个函数用于关闭进程及其子进程
std::map<DWORD, std::vector<DWORD>> GetProcessRelation();
void TerminateProcessTree(DWORD pid);

//发送http消息，200成功，其他失败
std::map<int, std::string> curlHttp(const QString& url, const QString& otherString = "{\"1\":\"1\"}");

//这两个函数用于获取mac地址
std::wstring Encode16(const BYTE* buf, int len);
std::wstring getWindowMacAddress();

std::wstring qToStdWString(const QString &str);
QString stdWToQString(const std::wstring &str);

//判断进程是否存在
BOOL isExistProcess(DWORD process_id);
#endif // UTILS_H
