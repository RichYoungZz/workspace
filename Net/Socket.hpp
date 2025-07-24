#pragma once

#include <sys/socket.h>
#include <errno.h>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>

#include "InetAddress.hpp"

namespace HumbleServer{

class Socket {
//函数定义
public:
    explicit Socket(int sockfd) : sockfd_(sockfd) {} //避免隐式转换，必须显式调用构造函数
    Socket() {sockfd_ = ::socket(AF_INET, SOCK_STREAM, 0);}; //也可以不由外部传入，自己创建一个socket
    ~Socket() { ::close(sockfd_); } //Socket对象被释放以后，其持有的sockfd也需要被关闭

    int getFd() const { return sockfd_; }

    int bind(const InetAddress& addr); //绑定
    int listen(int establishedQueueSize = 1024); //监听
    int accept(); //接受连接

//变量定义
public:
    int sockfd_;
    //在此不保存InetAddress，只专注套接字fd操作
};

}

/* 函数实现分割线---------------------------------------------------------------------------------------------------------------------------*/

/**
    * @brief 将套接字绑定到指定ip地址和端口
    * @param[in] addr 要绑定的地址
    * @return 0成功，非0失败
*/
int HumbleServer::Socket::bind(IN const InetAddress& addr) {
    int ret = ::bind(sockfd_, (struct sockaddr*)&addr.getAddr(), sizeof(sockaddr_in));
    int err = errno;
    if(ret != 0)
    {
        printf("bind error: %s\n", strerror(err));
    }
    else
    {
        printf("fd = %d, bind success\n", sockfd_);
    }
    return ret;
}

/**
    * @brief 监听套接字
    * @param[in] establishedQueueSize 已经建立的连接队列大小，默认1024
    * @return 0成功，非0失败
*/
int HumbleServer::Socket::listen(IN int establishedQueueSize) {
    int ret = ::listen(sockfd_, establishedQueueSize);
    int err = errno;
    if(ret != 0)
    {
        printf("listen error: %s\n", strerror(err));
    }
    else
    {
        printf("fd = %d, listen success\n", sockfd_);
    }
    return ret;
}

/**
    * @brief 接受连接
    * @return >0表示新连接对应的Fd，<=0表示出错，但仍可以继续accept
*/
int HumbleServer::Socket::accept() {
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    //这个addr不需要保存并传给Channel，因为这个可以后续可以通过getsockname，用socket来获取到对应的sockaddr
    int client_fd = ::accept(sockfd_, (struct sockaddr*)&addr, (socklen_t *)&addrlen);
    if(client_fd <= 0)
    {
        printf("accept error.\n");
    }
    else
    {
        printf("accept success, client_fd = %d\n", client_fd);
    }
    return client_fd;
}
