//
// Created by bcyz on 2024/2/4.
//

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif


#include "clipp.h"

#include <windows.h>
#include <winsock2.h>


#include <iostream>
#include <fstream>
#include <string>

#define DEFAULT_PORT 8233
#define DEFAULT_BUFLEN 512


void init_client(SOCKET *ConnectSocket, const char *ipAddr);

void init_recv(SOCKET *ClientSocket);

bool is_folder(const std::string &path);

void send_files();

void recv_files();

int main(int argc, char **argv)
{
    // 处理命令行参数

    using namespace clipp;
    enum class mode
    {
        send, recv, help
    };
    mode select = mode::help;
    bool r = false;
    std::string ip, path;

    //发送 : nft send [-r] path ip_address
    auto send_mode = (
            command("send").set(select, mode::send).doc("Send files to another computer running nft."),
                    option("-r").set(r),
                    value("path", path) & value("ip address", ip));
    //接收 : nft recv
    auto recv_mode = (
            command("recv").set(select, mode::recv).doc("Receive files from another computer.")
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
                send_files();
                break;
            case mode::recv:
                recv_files();
                break;
            case mode::help:
                std::cout << make_man_page(send_mode | recv_mode | help_mode);
                break;
        }
    }

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
    // From https://learn.microsoft.com/zh-cn/windows/win32/winsock/complete-client-code
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
 * 判断路径是否是文件夹
 */
bool is_folder(std::string &path)
{
    WIN32_FIND_DATA FindFileData;
    HANDLE hFind;
    hFind = FindFirstFileA(path.c_str(), (LPWIN32_FIND_DATAA) &FindFileData);
    if (hFind == INVALID_HANDLE_VALUE)
        exit(1);
    if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        FindClose(hFind);
        return true;
    } else
    {
        FindClose(hFind);
        return false;
    }
}

/*
 * 发送文件主逻辑
 */
void send_files()
{

}

/*
 * 接收文件主逻辑
 */
void recv_files()
{

}
