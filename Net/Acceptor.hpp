#pragma once

#include <sys/types.h>
#include <sys/socket.h>

#include "public.h"
#include "Socket.hpp"
#include "InetAddress.hpp"

namespace HumbleServer{

/**
* @brief 用于监听新连接，当有新连接时，接受并且注册到loop。【功能类】
*/
class  Acceptor{
//函数
public:
    Acceptor(EventLoop* loop, InetAddress& addr);
    ~Acceptor() = default;

    int start();
    void setNewConnectionCallback(EventCallback &callback);
    int accept(TimeStamp now);
//变量
private:
    EventLoop* loop_;
    std::shared_ptr<Socket> acceptSocket_; ///accpet需要关闭fd，放析构函数里面，由智能指针管理生命周期，自动关闭
    std::shared_ptr<Channel> acceptChannel_;
    EventCallback newConnectionCallback_;
};

}

/* 函数实现分割线---------------------------------------------------------------------------------------------------------------------------*/

HumbleServer::Acceptor::Acceptor(EventLoop* loop, InetAddress& addr)
{
    loop_ = loop;

    int sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
    int optval = 1;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)); //设置TCP_NODELAY
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)); //设置addr复用
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)); //设置port复用
    acceptSocket_ = std::make_shared<Socket>(sockfd);
    acceptSocket_->bind(addr);
    acceptSocket_->listen();

    acceptChannel_ = std::make_shared<Channel>(acceptSocket_);
    acceptChannel_->setEventLoop(loop_);
    acceptChannel_->setFocusEvent(EPOLLIN);
    acceptChannel_->setEventHandler(EPOLLIN, std::bind(&Acceptor::accept, this, std::placeholders::_1)); 
}

int HumbleServer::Acceptor::start(){
    acceptChannel_->updateInEventLoop(ChannelStatus_Added); //添加到epoll里面，开始等待新连接
    return Success; 
}

/**
* @brief 设置新连接回调，由TcpServer注册下来，该回调内容就是getNextLoop，并且把新socket注册到loop
*/
void HumbleServer::Acceptor::setNewConnectionCallback(EventCallback& callback){
    newConnectionCallback_ = callback; //必须先设置这个，才能调用start，否则没有对应callback处理函数
}

/**
* @brief 接受连接，返回新连接的fd
*/
int HumbleServer::Acceptor::accept(TimeStamp now){
    int connectFd = acceptSocket_->accept();
    printf("accept a new connection, fd = %d\n", connectFd);
    newConnectionCallback_(&connectFd); //回调，把新连接注册到loop，传入的是connfd的指针
    return connectFd;
}