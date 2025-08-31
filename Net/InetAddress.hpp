#pragma once

#include <netinet/in.h>
#include <string>
#include <unistd.h>
#include <signal.h>

#include "public.h"

namespace HumbleServer {

/**
* @brief 网络地址类，对 sockaddr_in 的封装【变量类】
*/
class InetAddress {
//函数
public:
    //构造函数
    InetAddress(uint16_t port);
    InetAddress(const std::string& ip, uint16_t port);
    InetAddress(const struct sockaddr_in& addr) : addr_(addr) {}
    std::string getIp() const;
    int getPort() const;
    sockaddr_in getAddr() const;

//变量
private:
    sockaddr_in addr_;
    int port_;
    std::string ip_;
};

}

/* 函数实现分割线---------------------------------------------------------------------------------------------------------------------------*/

/**
 * @brief 构造函数
 * 
 * @param[in]  port 输入参数 - 端口号
 */
HumbleServer::InetAddress::InetAddress(IN uint16_t port) {
    addr_.sin_family = AF_INET; //IPV4协议族
    addr_.sin_port = htons(port); //将端口转换为网络字节序
    addr_.sin_addr = inet_addr("127.0.0.1"); //将IP地址转换为网络字节序

    port_ = port;
    ip_ = "127.0.0.1";
}

/**
 * @brief 构造函数
 * 
 * @param[in]  ip   输入参数 - IP地址字符串
 * @param[in]  port 输入参数 - 端口号
 */
HumbleServer::InetAddress::InetAddress(IN const std::string& ip, IN uint16_t port) {
    addr_.sin_family = AF_INET; //IPV4协议族
    addr_.sin_port = htons(port); //将端口转换为网络字节序
    addr_.sin_addr = inet_addr(ip.c_str()); //将IP地址转换为网络字节序

    port_ = port;
    ip_ = ip;
}

sockaddr_in HumbleServer::getAddr() const {
    return addr_;
}

int HumbleServer::getPort() const {
    return port_;
}

std::string HumbleServer::getIp() const {
    return ip_;
}