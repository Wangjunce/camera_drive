#include "utils.h"

#include <windows.h>
#include <iphlpapi.h>
#include <tlhelp32.h>
#include <Mmdeviceapi.h>
#include <strmif.h>
#include <initguid.h>
#include <atlconv.h>
#include <dvdmedia.h>

#pragma comment(lib, "iphlpapi.lib")

#define VI_MAX_CAMERAS 20
#define MAXSIZE 512
DEFINE_GUID(CLSID_SystemDeviceEnum, 0x62be5d10, 0x60eb, 0x11d0, 0xbd, 0x3b, 0x00, 0xa0, 0xc9, 0x11, 0xce, 0x86);
DEFINE_GUID(CLSID_VideoInputDeviceCategory, 0x860bb310, 0x5d01, 0x11d0, 0xbd, 0x3b, 0x00, 0xa0, 0xc9, 0x11, 0xce, 0x86);
DEFINE_GUID(IID_ICreateDevEnum, 0x29840822, 0x5b84, 0x11d0, 0xbd, 0x3b, 0x00, 0xa0, 0xc9, 0x11, 0xce, 0x86);
DEFINE_GUID(CLSID_AudioInputDeviceCategory, 0x33d9a762, 0x90c8, 0x11d0, 0xbd, 0x43, 0x0, 0xa0, 0xc9, 0x11, 0xce, 0x86);
DEFINE_GUID(CLSID_AudioRendererCategory,0xe0f158e1,0xcb04,0x11d0,0xbd,0x4e,0x0,0xa0,0xc9,0x11,0xce,0x86);


std::map<std::string, std::string> getJsonInfo(const char* buff_data, int data_len)
{
    std::map<std::string, std::string> data;
    std::string key = "";
    std::string value = "";
    std::string value2 = "";
    std::string json_type = "key";
    for(int i = 0; i < data_len; i ++){
        if(buff_data[i] == '{' || buff_data[i] == ' '){
            //判断是否为value为字符串时的中间位置 在这种情况下就是图片字符串中的字符要给value+= 下同
            if(value != "" && value[0] == '"' && value[value.length()-1] != '"'){
                value += buff_data[i];
            }else{
                continue;
            }

        }else if(buff_data[i] == ':'){
            //判断:是否为value为字符串时的中间位置 在这种情况下要给value+=
            if(value != "" && value[0] == '"' && value[value.length()-1] != '"'){
                value += buff_data[i];
            }else{
                json_type = "value";
            }

        }else if(buff_data[i] == ',' || buff_data[i] == '}'){
            //判断是否为value为字符串时的中间位置 在这种情况下就是图片字符串中的字符要给value+=
            if(value != "" && value[0] == '"' && value[value.length()-1] != '"'){
                value += buff_data[i];
                continue;
            }

            //这里需要处理一对数据了
            //key需要把string去掉
            if(key.length() == 0){
                key = "";
                value = "";
                json_type = "key";
                continue;
            }
            key.erase(std::remove(key.begin(), key.end(), '"'), key.end());
            //value也需要去掉string,但去掉之前需要判断这是字符串还是整型或浮点
            //value是字符串
            if(key.length() == 0){
                value2 = "";
            }else if(value[0] == '"'){
                value.erase(std::remove(value.begin(), value.end(), '"'), value.end());
                value2 = value;
            }else{
                value2 = value;
            }
            data.emplace(key, value2);

            key = "";
            value = "";
            json_type = "key";
        }else{
            //当下的字符是key
            if(json_type == "key"){
                key += buff_data[i];
            }else{
                //当下的字符是value
                value += buff_data[i];
            }
        }
    }

    return data;
}

size_t req_reply(void* ptr, size_t size, size_t nmemb, void* stream)
{
    //在注释的里面可以打印请求流，cookie的信息
    //cout << "----->reply" << endl;
    std::string* str = (std::string*)stream;
    //cout << *str << endl;
    (*str).append((char*)ptr, size * nmemb);

    return size * nmemb;
}

void initialSDLwin(SdlLabel* l, int w_width, int w_height)
{
    l->m_win = SDL_CreateWindowFrom((void *)l->m_labelBody->winId());
    //    win = SDL_CreateWindow("Media Player",
    //                           SDL_WINDOWPOS_UNDEFINED,
    //                           SDL_WINDOWPOS_UNDEFINED,
    //                           w_width, w_height,
    //                           SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
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

//获取麦克风名称
QString getAudioDeviceName()
{
    char sName[256] = { 0 };
    QString capture = "";
    bool bRet = false;
    ::CoInitialize(NULL);

    ICreateDevEnum* pCreateDevEnum;//enumrate all audio capture devices
    HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum,
                                  NULL,
                                  CLSCTX_INPROC_SERVER,
                                  IID_ICreateDevEnum,
                                  (void**)&pCreateDevEnum);

    IEnumMoniker* pEm;
    hr = pCreateDevEnum->CreateClassEnumerator(CLSID_AudioInputDeviceCategory, &pEm, 0);
    if (hr != NOERROR)
    {
        ::CoUninitialize();
        return "";
    }

    pEm->Reset();
    ULONG cFetched;
    IMoniker *pM;
    while (hr = pEm->Next(1, &pM, &cFetched), hr == S_OK)
    {

        IPropertyBag* pBag = NULL;
        hr = pM->BindToStorage(0, 0, IID_IPropertyBag, (void**)&pBag);
        if (SUCCEEDED(hr))
        {
            VARIANT var;
            var.vt = VT_BSTR;
            hr = pBag->Read(L"FriendlyName", &var, NULL);//还有其他属性，像描述信息等等
            if (hr == NOERROR)
            {
                //获取设备名称
                WideCharToMultiByte(CP_ACP, 0, var.bstrVal, -1, sName, 256, "", NULL);
                capture = QString::fromLocal8Bit(sName);
                SysFreeString(var.bstrVal);
            }
            pBag->Release();
        }
        pM->Release();
        bRet = true;
    }
    pCreateDevEnum = NULL;
    pEm = NULL;
    ::CoUninitialize();

    return capture;
}

wchar_t* QString2Wchar(QString buf)
{
    return (wchar_t*)reinterpret_cast<const wchar_t *>(buf.utf16());
}

QString Wchar2QString(wchar_t *buf)
{
    return QString::fromWCharArray(buf);
}

//e:\ffmpeg-6.1-full_build-shared\bin\ffmpeg -f dshow -i audio="麦克风阵列 (Realtek(R) Audio)"
//-codec:a pcm_alaw -ac 1 -ar 8000 -f flv "rtmp://192.168.5.100:1935/live/test"
int my_CreateProcess(SHELLEXECUTEINFO& lpExecInfo, const QString& microphoneName, const QString& streamName)
{
    WCHAR name[MAXSIZE] = {0};
    wcscpy_s(name, QString2Wchar(microphoneName));
    WCHAR t[MAXSIZE] = L"/k ffmpeg -f dshow -i audio=\"";

    //WCHAR name1[] = L"\" -codec:a pcm_alaw -ac 1 -ar 8000 -f flv \"rtmp://192.168.5.102:1935/audio/wjc003_audio\"";

    QString name2 = "\" -codec:a pcm_alaw -ac 1 -ar 8000 -f flv \"rtmp://"+QString::fromUtf8(MEDIATALKURL)+"/audio/"+streamName+"\"";

    wcscat_s(t, MAXSIZE, name);
    wcscat_s(t, MAXSIZE, QString2Wchar(name2));

    //qDebug()<<t;

    //ShellExecute(NULL, L"open", L"cmd", t, NULL, SW_SHOWNORMAL);

    lpExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
    lpExecInfo.lpFile = L"cmd.exe";
    lpExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
    lpExecInfo.hwnd = NULL;
    lpExecInfo.lpVerb = NULL;
    lpExecInfo.lpParameters = t;
    lpExecInfo.lpDirectory = NULL;
    lpExecInfo.nShow = SW_SHOWNORMAL;

    return ShellExecuteEx(&lpExecInfo);
}

/*
使用快照获取进程的父子关系。

PROCESSENTRY321中有个数据成员为th32ParentProcessID，借此获取进程间的父子关系。
创建快照和查看过程2，有着1+2个关键函数(这仨函数搭配使用的，建议直接查看示例)：CreateToolhelp32Snapshot，Process32First，Process32Next
使用TerminateProcess3关闭进程，但由于该函数需要进程句柄所以还得使用函数OpenProcess4。
顺便一说的就是，TerminateProcess官方文档明说是强制终止进程，说的直白点就是该函数的清理工作做的很不到位，其中一点就是仅杀死父进程后子进程很可能会成为后台进程驻留在系统内(得开任务管理器手动清理)，所以使用该函数终止进程时需要连同子进程一并杀死以防止残留。

还有就是，使用快照遍历所有进程，看似很耗时间，实际上并没有，我一开始也以为要“遍历”会不会很慢，然后发现香的不行，所以不必担心造成卡顿。
原文链接：https://blog.csdn.net/weixin_44733774/article/details/130878452
*/
std::map<DWORD, std::vector<DWORD>> GetProcessRelation() {//获取进程父子关系。键值对含义为：pid:[P-pid,C-pid,C-pid,...]，(其中，P-Parent，C-Child)
    std::map<DWORD, std::vector<DWORD>>tree;

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);
        for (Process32First(hSnap, &pe32); Process32Next(hSnap, &pe32);) {
            DWORD curr = pe32.th32ProcessID;
            DWORD parent = pe32.th32ParentProcessID;
            std::vector<DWORD>& Vcurr = tree[curr];
            std::vector<DWORD>& Vparent = tree[parent];

            if (Vcurr.empty())
                Vcurr.push_back(0);
            if (Vparent.empty())
                Vparent.push_back(0);
            Vcurr[0] = parent;
            Vparent.push_back(curr);
        }
        CloseHandle(hSnap);
    }
    return tree;
}

void TerminateProcessTree(DWORD pid) {//终结以pid为根节点的进程树
    std::map<DWORD, std::vector<DWORD>> ret = GetProcessRelation();
    std::vector<DWORD>lst;
    std::vector<DWORD>stk = { pid };
    while (stk.empty() == false) {
        DWORD pid = stk.back();
        stk.pop_back();
        for (auto p = ret[pid].begin() + 1; p != ret[pid].end(); ++p) {
            lst.push_back(*p);
            stk.push_back(*p);
        }
    }
    //for (auto p = lst.begin(); p != lst.end(); ++p) {
    //    std::cout << *p << std::endl;
    //}
    for (auto p = lst.begin(); p != lst.end(); ++p) {
        HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, *p);
        if (hProc != INVALID_HANDLE_VALUE) {
            TerminateProcess(hProc, -1);
            WaitForSingleObject(hProc, INFINITE);
            CloseHandle(hProc);
        }
    }
}

std::map<int, std::string> curlHttp(const QString& url, const QString& otherString)
{
    /*
     * 用visual studio生成的debug模式的库报链接错误，不知道是不是版本不对
     * 用dll的方式编译好像可以
    */
    CURL* curl = curl_easy_init();
    CURLcode res_code;
    std::string response = "";
    if(curl){
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
        curl_easy_setopt(curl, CURLOPT_URL, (QString::fromUtf8(SIPHTTPURL)+QString::fromUtf8("/")+url).toStdString().c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "http");

        //这里就得分两步，否则会被提前析构了造成乱码
        QByteArray qdata_b = otherString.toUtf8();
        const char* qdata_p = qdata_b.data();
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, qdata_p); // 要发送的数据

        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "User-Agent: shexiangtou_client");
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &req_reply);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&response);
        res_code = curl_easy_perform(curl);

        if (res_code != CURLE_OK) {
            char errorFailed[1024] = {};
            sprintf(errorFailed, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res_code));
            //fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res_code));
            curl_easy_cleanup(curl);
            return std::map<int, std::string>{{-1, errorFailed}};
        }
        else {

            return std::map<int, std::string>{{200, response}};
        }
        curl_easy_cleanup(curl);
    }
    else{
        return std::map<int, std::string>{{-2, "curl 初始化失败"}};
    }
}

std::wstring Encode16(const BYTE* buf, int len)
{
    const WCHAR strHex[] = L"0123456789ABCDEF";
    std::wstring wstr;
    wstr.resize(len * 2);
    for (int i = 0; i < len; ++i)
    {
        wstr[i * 2 + 0] = strHex[buf[i] >> 4];
        wstr[i * 2 + 1] = strHex[buf[i] & 0xf];
    }
    return wstr;
}

std::wstring getWindowMacAddress()
{
    ULONG ulBufferSize = 0;
    DWORD dwResult = ::GetAdaptersInfo(NULL, &ulBufferSize);
    if (ERROR_BUFFER_OVERFLOW != dwResult) {
        return L"";
    }

    PIP_ADAPTER_INFO pAdapterInfo = (PIP_ADAPTER_INFO) new BYTE[ulBufferSize];
    if (!pAdapterInfo)
    {
        return L"";
    }

    dwResult = ::GetAdaptersInfo(pAdapterInfo, &ulBufferSize);
    if (ERROR_SUCCESS != dwResult)
    {
        delete[] pAdapterInfo;
        return L"";
    }

    BYTE pMac[MAX_ADAPTER_ADDRESS_LENGTH] = { 0 };
    int nLen = MAX_ADAPTER_ADDRESS_LENGTH;

    if (NULL != pAdapterInfo)
    {
        PIP_ADDR_STRING pAddTemp = &(pAdapterInfo->IpAddressList);

        if (NULL != pAddTemp)
        {
            for (int i = 0; i < (int)pAdapterInfo->AddressLength; ++i)
            {
                pMac[i] = pAdapterInfo->Address[i];
            }
            nLen = pAdapterInfo->AddressLength;
        }
    }
    delete[] pAdapterInfo;

    std::wstring strMac = Encode16(pMac, nLen);
    return strMac;
}

/*! Convert an QString to a std::wstring */
std::wstring qToStdWString(const QString &str)
{
#ifdef _MSC_VER
    return std::wstring((const wchar_t *)str.utf16());
#else
    return str.toStdWString();
#endif
}

/*! Convert an std::wstring to a QString */
QString stdWToQString(const std::wstring &str)
{
#ifdef _MSC_VER
    return QString::fromUtf16((const ushort *)str.c_str());
#else
    return QString::fromStdWString(str);
#endif
}

//判断进程是否存在
BOOL isExistProcess(DWORD process_id)
{
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (INVALID_HANDLE_VALUE == hSnapshot) {
        return NULL;
    }
    PROCESSENTRY32 pe = { sizeof(pe) };
    for (BOOL ret = Process32First(hSnapshot, &pe); ret; ret = Process32Next(hSnapshot, &pe)) {

        if (pe.th32ProcessID == process_id)
        {
            return TRUE;
        }
    }
    CloseHandle(hSnapshot);
    return FALSE;
}
