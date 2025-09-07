#pragma once

#include "public.h"
#include "Acceptor.hpp"
#include "EventLoop.hpp"
#include "EventLoopThreadPool.hpp"
#include "TcpConnection.hpp"

#include <unordered_map>
#include <memory>

namespace HumbleServer{

/**
* @brief TcpServer  服务器类，持有一个Acceptor监听链接，多个EventLoop管理EpollPoller和Channel，去对每个fd进行处理。还持有若干TcpConnection一一对应Channel。【功能类】
*/
class TcpServer{
//函数
public:
    TcpServer(std::shared_ptr<EventLoop> mainLoop, const InetAddress& listenAddr);
    ~TcpServer() = default;

    int newConnection(int sockfd); //注册给Acceptor的回调函数，用于新链接的处理，epoll接受到EPOLLIN事件后调用，创建TcpConnection
    int removeTcpConnection();  //注册给TcpConnection的回调函数，用于TcpConnection关闭后，通知TcpServer从TcpConnectionMap_中删除

    void setMessageCallback(messageCallback cb) { messageCallback_ = cb; } //暂存用户注册的回调函数，创建TcpConnection时赋值给TcpConnection
    void setConnectionCallback(connectionCallback cb) { connectionCallback_ = cb; } //暂存用户注册的回调函数，创建TcpConnection时赋值给TcpConnection
    void setWriteCompleteCallback(writeCompleteCallback cb) { writeCompleteCallback_ = cb; } //暂存用户注册的回调函数，创建TcpConnection时赋值给TcpConnection
    void setHighWaterMarkCallback(highWaterMarkCallback cb) { highWaterMarkCallback_ = cb; } //暂存用户注册的回调函数，创建TcpConnection时赋值给TcpConnection

    int setThreadsNum(int numThreads) ; //设置线程池中线程的数量，必须在start()之前调用
    void start(); //启动服务器，启动Acceptor，启动线程池及其中的subLoop
//变量
private:
    std::unordered_map<int, std::shared_ptr<TcpConnection> > TcpConnectionMap_;

    std::shared_ptr<Acceptor>  acceptor_;   // 监听新链接的实例
    std::shared_ptr<EventLoop> mainLoop_;   // 主EventLoop，除了监听新链接，也可以处理用户注册其他回调，也可以多个TcpServer共享一个EventLoop。该mainLoop在TcpServer外部创建，传入TcpServer。
    std::shared_ptr<EventLoopThreadPool> threadPool_; //线程池，管理subLoop

    messageCallback messageCallback_; // 用户注册的消息回调，业务逻辑处理
    connectionCallback connectionCallback_; // 用户注册的新链接连接完成后回调，业务逻辑处理
    writeCompleteCallback writeCompleteCallback_; // 用户注册的写完回调，业务逻辑处理
    highWaterMarkCallback highWaterMarkCallback_; // 用户注册的写缓冲区高水位回调

    InetAddress listenAddr_; 
};

}

/* 函数实现分割线---------------------------------------------------------------------------------------------------------------------------*/
/**
* @brief TcpServer构造函数，创建Acceptor和线程池
*/
HumbleServer::TcpServer::TcpServer(std::shared_ptr<HumbleServer::EventLoop> mainLoop, const InetAddress& listenAddr)
{
    if(loop == nullptr)
    {
        printf("TcpServer::TcpServer() error: loop is nullptr\n");
        assert(0);
    }
    mainLoop_ = mainLoop;
    listenAddr_ = listenAddr;
    acceptor_ = std::make_shared<Acceptor>(mainLoop_, listenAddr_);
    threadPool_ = std::make_shared<EventLoopThreadPool>(mainLoop_);

    //Accpetor配置
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this, std::placeholders::_1));
}

/**
* @brief Accpetor发现新链接以后的回调函数，创建TcpConnection并分配给subLoop
* @param sockfd 新链接的socketfd
*/
int HumbleServer::TcpServer::newConnection(int sockfd)
{
    EventLoop* subLoop = threadPool_->getNextLoopFromRoundRobin();
    std::shared_ptr<TcpConnection> newTcpConnection = std::make_shared<TcpConnection>(subLoop, sockfd);
    TcpConnectionMap_[sockfd] = newTcpConnection;
    if(messageCallback_ != nullptr)
    {
        newTcpConnection->setMessageCallback(messageCallback_);
    }
    if(connectionCallback_ != nullptr)
    {
        newTcpConnection->setConnectionCallback(connectionCallback_);
    }
    if(writeCompleteCallback_ != nullptr)
    {
        newTcpConnection->setWriteCompleteCallback(writeCompleteCallback_);
    }
    newTcpConnection->setOnCloseCallback(std::bind(&TcpServer::removeTcpConnection, this));

    subLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, newTcpConnection)); //connectEstablished是让TcpConnection开始监听，需要立即执行

    return Success;
}

/**
* @brief TcpConnection关闭后，从TcpConnectionMap_中删除该TcpConnection
*/
int HumbleServer::TcpServer::removeTcpConnection(TcpConnection& tcpConnection)
{
    TcpConnectionMap_.erase(tcpConnection.getFd());
    return Success;
}

/**
* @brief 设置线程池中线程的数量，必须在start()之前调用
*/
int HumbleServer::TcpServer::setThreadsNum(int numThreads)
{
    int ret = Success;
    if(threadPool_ != nullptr)
    {
        ret = threadPool_->setThreadsNum(numThreads);
        if(ret != Success)
        {
            printf("TcpServer::setThreadsNum() error: threadPool_->setThreadsNum() error\n");
        }
    }
    else
    {
        ret = Fail;
        printf("TcpServer::setThreadsNum() error: threadPool_ is nullptr\n");
    }
    return ret;
}

/**
* @brief 启动服务器，启动Acceptor，启动线程池及其中的subLoop
*/
void HumbleServer::TcpServer::start()
{
    threadPool_->start();
    acceptor_->start();
}

