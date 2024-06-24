//
// Created by bcyz on 2024/2/4.
//

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif


#define NOMINMAX   // 解决minwindef.h中max,min冲突
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "clipp.h"

#include <windows.h>
#include <winsock2.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <format>
#include <thread>
#include <atomic>
#include <ctime>
#include <cmath>
#include <vector>
#include <filesystem>
#include <exception>

#define DEFAULT_PORT 8233
#define DEFAULT_BUFLEN 1024

using std::cout, std::cin, std::cerr, std::endl,
      std::thread, std::vector, std::string,
      std::atomic_size_t, std::stringstream, std::fstream ;

namespace fs = std::filesystem;

struct file_info{
    string name;
    size_t len=0;
    bool   is_folder=false;
    bool operator == (const file_info &f)const
    {
        return name==f.name;
    }
};

void init_client(SOCKET *ConnectSocket, const char *ipAddr);

void init_recv(SOCKET *ClientSocket);



void show_progress();

void confirmation_acceptance(vector<file_info> & files);

bool send_single_file(SOCKET ConnectSocket, fstream & file);

bool recv_single_file(SOCKET ClientSocket,  fstream & file);

void send_files(const string & ip, const std::vector<string> & paths);

void recv_files(const fs::path & path);

vector<file_info> split_data(const char * data, int len);

string convert_unit(size_t size);

inline size_t folder_size(const fs::path& p)
{
    std::filesystem::directory_entry d{p};
    size_t size = 0;
    for (const fs::directory_entry& dir_entry :
            fs::recursive_directory_iterator(p))
    {
        size+=dir_entry.file_size();
    }

    return size;
}

// 字符串转数字
inline size_t ston(const string& s)
{
    size_t r;
    stringstream i(s);
    i>>r;
    return r;
}


char recv_buf[DEFAULT_BUFLEN];
char send_buf[DEFAULT_BUFLEN];

atomic_size_t have_done_len, file_len;
string name;
std::atomic_bool exit_f = false;


int main(int argc, char **argv)
{
    // 处理命令行参数

    using namespace clipp;
    enum class mode
    {
        send, recv, help
    };
    mode select = mode::help;
    std::string ip;
    string path;
    std::vector<string> paths;

    //发送 : nft send ip_address path
    auto send_mode = (
            command("send").set(select, mode::send).doc("Send files to another computer running nft."),
            value("ip address", ip),
            values("paths", paths)
    );
    //接收 : nft recv
    auto recv_mode = (
            command("recv").set(select, mode::recv).doc("Receive files from another computer."),
            opt_value("path", path).doc("the paths to save file")
    );
    //帮助 : nft help
    auto help_mode = (
            command("help").set(select, mode::help)
    );

    if (parse(argc, argv, send_mode | recv_mode | help_mode))
    {
        switch (select)
        {
            case mode::send:
                paths = (paths.empty() ? std::vector<string>(1,".") : paths);
                //转换为绝对路径
                std::for_each(paths.begin(), paths.end() ,
                              [](auto& dir_entry){dir_entry=fs::absolute(dir_entry).string();});
                send_files(ip, paths);
                break;
            case mode::recv:
                path = (path.empty() ? "." : path);
                recv_files(path);
                break;
            case mode::help:
                std::cout << make_man_page(send_mode | recv_mode | help_mode);
                break;
        }
    }else cerr << "Please enter valid arguments\n"
        << "Here are some possible arguments:\n"
        << usage_lines(send_mode|recv_mode|help_mode, "nft") << '\n';

    return 0;
}

/*
 * 初始化发送端
 */
void init_client(SOCKET *ConnectSocket, const char *ipAddr)
{
    WSADATA wsaData;

    int iResult;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0)
    {
        printf("WSAStartup failed: %d\n", iResult);
        exit(1);
    }

    *ConnectSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (*ConnectSocket == INVALID_SOCKET)
    {
        printf("Error at socket(): %d\n", WSAGetLastError());
        WSACleanup();
        exit(1);
    }

    struct sockaddr_in saServer{};
    saServer.sin_family = AF_INET;
    saServer.sin_addr.s_addr = inet_addr(ipAddr);
    saServer.sin_port = htons(DEFAULT_PORT);

    iResult = connect(*ConnectSocket, (sockaddr *) &saServer, sizeof(saServer));
    if (iResult == SOCKET_ERROR)
    {
        closesocket(*ConnectSocket);
        *ConnectSocket = INVALID_SOCKET;
    }
}

/*
 * 初始化接收端
 */
void init_recv(SOCKET *ClientSocket)
{
    WSADATA wsaData;

    int iResult;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0)
    {
        printf("WSAStartup failed: %d\n", iResult);
        exit(1);
    }

    *ClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (*ClientSocket == INVALID_SOCKET)
    {
        printf("Error at socket(): %d\n", WSAGetLastError());
        WSACleanup();
        exit(1);
    }

    struct sockaddr_in saServer{};
    saServer.sin_family = AF_INET;
    saServer.sin_addr.s_addr = inet_addr("0.0.0.0");
    saServer.sin_port = htons(DEFAULT_PORT);

    iResult = bind(*ClientSocket, (sockaddr *) &saServer, sizeof(saServer));
    if (iResult == SOCKET_ERROR)
    {
        printf("bind failed with error: %d\n", WSAGetLastError());
        closesocket(*ClientSocket);
        WSACleanup();
        exit(1);
    }
}

/*
 * 发送单个文件
 */
bool send_single_file(SOCKET ConnectSocket,  fstream & file)
{
    int iResult;

    file.seekg(0, std::ios::beg);

    have_done_len = 0;
    thread progress_bar {show_progress};

    // 发送文件
    while (!file.eof())
    {
        file.read(send_buf, DEFAULT_BUFLEN);
        iResult = send(ConnectSocket, send_buf, (int)file.gcount(), 0);
        if (iResult == SOCKET_ERROR)
        {
            printf("send failed with error: %d\n", WSAGetLastError());
            closesocket(ConnectSocket);
            WSACleanup();
            return false;
        }
        have_done_len += file.gcount();
        ZeroMemory(send_buf, DEFAULT_BUFLEN);

    }
    file.close();
    progress_bar.join();

    return true;
}

/*
 * 接收单个文件
 */
bool recv_single_file(SOCKET ClientSocket, fstream & file)
{
    int iResult;

    file.seekg(0, std::ios::beg);

    if (!file.is_open())
    {
        std::cout << "文件打开失败\n";
        return false;
    }
    // 显示进度条
    have_done_len = 0;

    thread progress_bar {show_progress};
    // Receive until the peer shuts down the connection
    do
    {
        iResult = recv(ClientSocket, recv_buf, DEFAULT_BUFLEN, 0);
        have_done_len += iResult;
        if (iResult >= 0)
        {
            file.write(recv_buf, iResult);
            ZeroMemory(recv_buf, DEFAULT_BUFLEN);
        }else
        {
            cout<< "\033[31m" << "\nAccidental disconnection." << "\033[0m";
            file.close();
            exit_f = true;
            progress_bar.join();
            exit_f = false;
            return false;
        }
        if (have_done_len == file_len)
        {
            file.close();
            break;
        }

    } while (true);
    progress_bar.join();
    return true;
}

/*
 * 发送文件主逻辑
 */
void send_files(const std::string & ip, const std::vector<std::string> & paths)
{
    auto ConnectSocket = INVALID_SOCKET;
    int iResult;

    init_client(&ConnectSocket, ip.c_str());

    int success = 0, failed = 0;
    std::string file_data;

    string data;

    for(const auto &path:paths)
    {
        fs::path p(path);
        data += std::format("{}\x01f{}{}\x01c",p.filename().string(),
                         fs::is_directory(p) ? folder_size(p) : fs::file_size(p),
                            (int)fs::is_directory(p));
    }


    cout<<data<<endl;
    // 发送文件信息
    iResult = send(ConnectSocket, data.c_str(), (int)data.length(), 0);
    if (iResult == SOCKET_ERROR)
    {
        cerr << "send filed with error: " << WSAGetLastError() << endl;
        closesocket(ConnectSocket);
        WSACleanup();
        exit(1);
    }

     // 接受反馈
    iResult = recv(ConnectSocket, recv_buf, DEFAULT_BUFLEN, 0);
    if (iResult < 0)
    {
        cerr << "linkage interrupt with error: " << WSAGetLastError() << endl;
    }

    vector<file_info> files;
    files = split_data(recv_buf,iResult);

    if(files.empty())
    {
        cout<<"all of the files are rejected.\n";
        failed=(int)paths.size();
    }

    cout<<"be ready to send: ";
    std::for_each(files.begin(), files.end(), [](const auto&f){cout<<f.name<<"; ";});
    cout<<endl;



    /*// 结束传输
    cout << "Transmission complete, " << success <<" success, " << failed << " filed.";
    iResult = send(ConnectSocket, "done", 4, 0);
    if (iResult == SOCKET_ERROR)
    {
        cout << "send end message filed with error: " << WSAGetLastError() << endl;
        closesocket(ConnectSocket);
        WSACleanup();
        exit(1);
    }
    // 等待结束
    iResult = recv(ConnectSocket, recv_buf, DEFAULT_BUFLEN, 0);*/

    cout << "Transmission complete, " << success <<" success, " << failed << " filed.";
}

/*
 * 接收文件主逻辑
 */
void recv_files(const fs::path & path)
{
    auto ListenSocket = INVALID_SOCKET;
    auto ClientSocket = INVALID_SOCKET;
    int iResult;

    int success = 0, failed = 0;

    string file_data;
    init_recv(&ListenSocket);
    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR)
    {
        std::cout << "\033[31m" << "listen failed with error: \n" << WSAGetLastError() << "\033[0m";
        closesocket(ListenSocket);
        WSACleanup();
        exit(1);
    }
    cout << "Waiting connection...\n";
    // Accept a client socket
    struct sockaddr_in sa{};
    int socklen = sizeof(sa);

    ClientSocket = accept(ListenSocket, (sockaddr *)&sa, &socklen);
    string ip = inet_ntoa(sa.sin_addr);
    if (ClientSocket == INVALID_SOCKET)
    {
        std::cout << "\033[31m" << "accept failed with error: " << WSAGetLastError() << "\033[0m\n";
        closesocket(ListenSocket);
        WSACleanup();
        exit(1);
    }
    closesocket(ListenSocket);
    cout << "Connection from " << ip << endl;

    vector<file_info> files;
    iResult = recv(ClientSocket, recv_buf, DEFAULT_BUFLEN, 0);
    if (iResult <= 0)
    {
        cout << "\033[31m" << "linkage interrupt with error: " << WSAGetLastError() << "\033[0m\n";
        exit(1);
    }

    files = split_data(recv_buf,iResult);
    confirmation_acceptance(files);

    string data;
    for(const auto &file:files)
    {
        data += std::format("{}\x01f{}{}\x01c",file.name, file.len, (int)file.is_folder);
    }
    data = data.empty()?"None":data;

    // 发送文件信息
    iResult = send(ClientSocket, data.c_str(), (int)data.length(), 0);
    if (iResult == SOCKET_ERROR)
    {
        cerr << "send filed with error: " << WSAGetLastError() << endl;
        closesocket(ClientSocket);
        WSACleanup();
        exit(1);
    }

    for(const auto &f:files)
    {
        if(!f.is_folder)


    }

    iResult = recv(ClientSocket, recv_buf, DEFAULT_BUFLEN, 0);

    for (auto const &r : files)
    {
        std::cout<<r.name<<"|"<<r.len<<"|"<<r.is_folder<<std::endl;
    }
}

void confirmation_acceptance(vector<file_info> & files)
{
    cout<<"Do you want to recv these files?\n";
    for (int i = 0; i < files.size(); ++i)
    {
        cout<<i<<") "<<files[i].name <<convert_unit(files[i].len)<<(files[i].is_folder?"folder":"file")<<endl;
    }
    cout<<"Input yes(y)/no(n)/Enter(=yes) or choice(c) to choice one by one: ";
    string answer;
    cin>>answer;
    switch (answer[0])
    {
        case 'n':
        case 'N': files.clear();
                    break;
        case 'y':
        case 'Y':
            break;
        case 'c':
            vector<file_info> f;
            int a;
            cout<<"input the number before the file you want to recv('q' to quit)";
            while(true)
            {
                cout<<": ";
                cin>>answer;
                if (answer[0] == 'q') break;
                try
                {
                    a = std::stoi(answer);
                } catch (std::exception &e)
                {
                    cerr << "invalid input\n";
                    answer.clear();
                    continue;
                }
                if (a < 0 || a >= files.size() || std::find(f.begin(), f.end(), files[a]) != f.end())
                {
                    cerr << "invalid input\n";
                    answer.clear();
                    continue;
                }
                f.push_back(files[a]);
            }
            files=f;
            break;
    }
}

/*
 * 切分包含文件数据的字符串
 */
vector<file_info> split_data(const char * data,int len)
{
    vector<file_info> result;
    file_info f;

    if(strcmp(data,"None") == 0)
    {
        return result;
    }

    int j=0;
    for (int i = 0; i < len; ++i)
    {
        if(data[i]=='\x01f')
        {
            f.name=string(data,j,i-j);
            j=i;
        }else if(data[i]=='\x01c')
        {
            f.len=std::stoull(string(data,j+1,i-j-2));
            f.is_folder=(bool)std::stoi(string(data,i-1,1));
            j=i+1;
            result.push_back(f);
        }
    }

    return result;
}

//生成进度条
//生成进度条 a.file
//         [====>               ]20% 100MB/500MB 10MB/s 00:40
void show_progress()
{
    Sleep(100);
    string  progress;
    clock_t time_now ,time_last = 0;
    double  speed, percentage;
    size_t  estimated_sec, last_have_done_len = 0;
    string  have_done_size_t, file_len_t;
    string progress_bar;

    cout<<name<<endl;
    while(true)
    {
        time_now            = clock();
        speed               = (double)(have_done_len-last_have_done_len)/((double)(time_now-time_last)/CLOCKS_PER_SEC);  //单位是字节\秒
        if(speed == 0)
        {
            speed = 10000; // 莫名奇妙的错bug，有概率speed会是0
        }
        last_have_done_len  = have_done_len;
        time_last           = time_now;
        percentage          = ((double)have_done_len/(double)file_len)*100;
        estimated_sec       = (file_len-have_done_len)/(size_t)speed;
        have_done_size_t    = convert_unit(have_done_len.load());      // 转换单位
        file_len_t          = convert_unit(file_len.load());           // 转换单位

        progress_bar.clear();
        for(int i=0; i<(int)percentage/5;i++)
        {
            progress_bar.append("=");
        }
        progress_bar.append(">");
        progress = std::format("[{: <21}]{:.2f}% {}/{} {}/s est:{}s",
                               progress_bar,
                               percentage,
                               have_done_size_t,
                               file_len_t,
                               convert_unit((long long)speed),
                               estimated_sec);

        cout<<"\033[K\r"<<progress;
        if(have_done_len == file_len || exit_f)
        {
            cout << endl;
            break;
        }
        Sleep(100);
    }
}

string convert_unit(size_t size)
{
    std::string result;
    const std::string units[5] = {"B" ,"KB", "MB", "GB", "PB"};
    int i;
	for (i=1; ((double)size/pow(1024,i))>1.0 ;++i)
        ;
    double a =(double)size/pow(1024,i-1);
	result = std::format("{:.2f}{}",a, units[i-1]);


    return result;
}