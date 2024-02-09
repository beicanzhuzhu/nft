#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

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

int main(int argc, char **argv)
{

    switch (argc)
    {
        case 1:
            std::cout << "error\n";
            break;
        case 2:
        {
            auto ListenSocket = INVALID_SOCKET;
            auto ClientSocket = INVALID_SOCKET;
            int iResult;
            // 发送缓存
            char send_buf[DEFAULT_BUFLEN];
            // 接收缓存
            char recv_buf[DEFAULT_BUFLEN];
            std::string file_data;
            init_recv(&ListenSocket);

            iResult = listen(ListenSocket, SOMAXCONN);
            if (iResult == SOCKET_ERROR)
            {
                printf("listen failed with error: %d\n", WSAGetLastError());
                closesocket(ListenSocket);
                WSACleanup();
                return 1;
            }

            // Accept a client socket
            ClientSocket = accept(ListenSocket, nullptr, nullptr);
            if (ClientSocket == INVALID_SOCKET)
            {
                printf("accept failed with error: %d\n", WSAGetLastError());
                closesocket(ListenSocket);
                WSACleanup();
                return 1;
            }
            closesocket(ListenSocket);

            iResult = recv(ClientSocket, recv_buf, DEFAULT_BUFLEN, 0);
            if (iResult <= 0)
            {
                std::cout << "发送端终止了链接\n";
            }

            file_data = recv_buf;

            size_t len = file_data.find('&');
            std::string name = file_data.substr(0, len);
            std::string file_len = file_data.substr(len+1, file_data.length());

            while (true)
            {
                std::cout<< "是否接受文件 " << name << " 大小 " << file_len << " bytes ?(yes/no):";
                std::string answer;
                std::cin >> answer;
                if (answer == "yes")
                {
                    iResult = send(ClientSocket, "yes", 3, 0);
                    if (iResult == SOCKET_ERROR)
                    {
                        printf("send failed with error: %d\n", WSAGetLastError());
                        closesocket(ClientSocket);
                        WSACleanup();
                        exit(1);
                    }
                    break;
                }
                else if (answer == "no")
                {
                    iResult = send(ClientSocket, "no", 2, 0);
                    closesocket(ClientSocket);
                    WSACleanup();
                    exit(1);
                }
                else
                {
                    std::cout << "输入 yes 或 no" <<std::endl;
                }
            }

            std::fstream file;
            file.open(name.c_str(), std::ios::out | std::ios::binary);
            if (!file.is_open())
            {
                std::cout << "文件打开失败\n";
                exit(1);
            }

            // Receive until the peer shuts down the connection

            long long have_recv_length = 0;
            do
            {

                iResult = recv(ClientSocket, recv_buf, DEFAULT_BUFLEN, 0);
                if (iResult > 0)
                {
                    file.write(recv_buf, iResult);
                    have_recv_length += iResult;
                    file.seekp(have_recv_length, std::ios::beg);
                    ZeroMemory(recv_buf, DEFAULT_BUFLEN);
                }
                else if (iResult == 0)
                    std::cout << "finish\n" << have_recv_length << " bytes have been received.";
                else
                {
                    std::cout<<"传输时发生错误\n";
                }
            } while (iResult > 0);

            file.close();

            break;
        }

        case 4:
        {

            auto ConnectSocket = INVALID_SOCKET;
            int iResult;
            // 发送缓存
            char send_buf[DEFAULT_BUFLEN];
            // 接收缓存
            char recv_buf[DEFAULT_BUFLEN];

            ZeroMemory(send_buf, DEFAULT_BUFLEN);


            std::string file_data;
            unsigned long long int file_len;

            if (strcmp(argv[1], "send") != 0)
            {
                std::cout << "error\n";
                return 1;
            }
            // 要发送的文件
            std::fstream file;
            file.open(argv[2], std::ios::in | std::ios::binary);
            if (!file.is_open())
            {
                std::cout << "文件打开失败\n";
                exit(1);
            }

            // 初始化套接字
            init_client(&ConnectSocket, argv[3]);

            file.seekg(0, std::ios::end);
            file_len = file.tellg();

            file_data = argv[2] + (std::string) "&" + std::to_string(file_len);
            std::cout << file_data << std::endl;
            file.seekg(0, std::ios::beg);
            // 发送文件名和文件大小
            iResult = send(ConnectSocket, file_data.c_str(), (int) file_data.length(), 0);
            if (iResult == SOCKET_ERROR)
            {
                printf("send failed with error: %d\n", WSAGetLastError());
                closesocket(ConnectSocket);
                WSACleanup();
                exit(1);
            }
            // 接收接收端反馈
            iResult = recv(ConnectSocket, recv_buf, DEFAULT_BUFLEN, 0);
            if (iResult <= 0)
            {
                std::cout << "接收端终止了链接\n";
            }

            printf("begin\n");
            // 接收端反馈是否接收
            if (strcmp(recv_buf, "yes") == 0)
            {
                std::cout << "begin send" << std::endl;
            } else if (strcmp(recv_buf, "no") == 0)
            {
                std::cout << "fail" << std::endl;
            }

            // 发送文件
            long long have_reade_length = 0;
            while (have_reade_length < file_len)
            {
                int send_len = (have_reade_length + DEFAULT_BUFLEN) < file_len ? DEFAULT_BUFLEN : (int) (file_len -
                                                                                                         have_reade_length);
                file.read(send_buf, send_len);
                iResult = send(ConnectSocket, send_buf, send_len, 0);
                if (iResult == SOCKET_ERROR)
                {
                    printf("send failed with error: %d\n", WSAGetLastError());
                    closesocket(ConnectSocket);
                    WSACleanup();
                    exit(1);
                }
                have_reade_length += send_len;
                file.seekg(have_reade_length, std::ios::beg);
                ZeroMemory(send_buf, DEFAULT_BUFLEN);
            }
            std::cout << "finish\n" << have_reade_length << " bytes have been sent.";

            // cleanup
            closesocket(ConnectSocket);
            WSACleanup();
            break;
        }
        default:
            std::cout<<"error\n";
            break;
    }
    return 0;
}

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

    iResult = connect(*ConnectSocket, (sockaddr *)&saServer, sizeof(saServer));
    if (iResult == SOCKET_ERROR)
    {
        closesocket(*ConnectSocket);
        *ConnectSocket = INVALID_SOCKET;
    }
}

bool is_folder(std::string &path)
{
    WIN32_FIND_DATA FindFileData;
    HANDLE hFind;
    hFind = FindFirstFileA(path.c_str(), (LPWIN32_FIND_DATAA)&FindFileData);
    if (hFind == INVALID_HANDLE_VALUE)
        exit(1);
    if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        FindClose(hFind);
        return true;
    }
    else
    {
        FindClose(hFind);
        return false;
    }
}

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

    iResult = bind(*ClientSocket, (sockaddr *)&saServer, sizeof(saServer));
    if (iResult == SOCKET_ERROR)
    {
        printf("bind failed with error: %d\n", WSAGetLastError());
        closesocket(*ClientSocket);
        WSACleanup();
        exit(1);
    }
}
