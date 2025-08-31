#pragma once

#include <memory>
#include <string>
#include <errno>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "public.h"
#include "Channel.hpp"
#include "Buffer.hpp"
#include "EventLoop.hpp"
#include "Socket.hpp"

namespace HumbleServer {

/**
@brief TcpConnection TCP连接类，每个TcpConnection对应一个客户端连接【功能类/变量类】整合了Channel以及收发缓冲区，功能很多，但是对TcpServer来说更像一个变量而已
1.需要有对应的SocketFd，进来以后新建Channel
2.需要有接收缓冲区InputBuffer和发送缓冲池OutputBuffer
3.需要知道其所在的EventLoop指针
4.需要有send函数，提供给Channel调用来发送数据。
5.需要有read函数，提供给Channel调用来接收数据以后处理，还可以有close、error、connect(这三个可选，非必需)，connect虽然我看都设置了但是好像没什么实际作用
6.如果要实现心跳检测，还需要有心跳检测定时器（可选，非必需）
*/
class TcpConnection {
//函数
public:
    TcpConnection(EventLoop* loop, int SocketFd);
    ~TcpConnection(); //这个不能使用default，需要释放掉Fd和Channel所占用的资源

    int send(const std::string& message); //主动调用发送数据
    int sendFile(const std::string& filename); //发送文件 //暂不实现，当前接口很有用，先预留在此，有一个sendFile的系统接口，可以直接发送文件内容。这样不需要调用send，避免数据多次拷贝。

    int handleRead(TimeStamp now); //处理数据接收
    int handleWrite(TimeStamp now); //处理数据发送，send发现系统TCP缓冲区满后，会触发EPOLLOUT事件回调handleWrite被动发送
    int handleClose(TimeStamp now); //处理连接关闭 //暂不实现
    int handleError(TimeStamp now); //处理连接错误 //暂不实现

    void setOnMessageCallback(EventCallback cb) { onMessageCallback_ = cb; } //设置收到数据回调
    void setOnWriteCompleteCallback(EventCallback cb) { onWriteCompleteCallback_ = cb; } //设置发送完成回调
    void setOnCloseCallback(EventCallback cb) { onCloseCallback_ = cb; } //设置连接关闭回调
    void setOnErrorCallback(EventCallback cb) { onErrorCallback_ = cb; } //设置连接错误回调

    int connectFinishCallBack(); //连接建立后，baseLoop执行的回调。主要是TcpConnection对象创建后，在TcpSever里面设置好回调以后，再通过该callback使能Read事件，开始接收数据。
//变量
private:
    HumbleServer::Buffer outputBuffer_; //发送缓冲区，send调用满了以后，会放到这里且注册EPOLLOUT，等待系统TCP缓冲区满以后触发EPOLLOUT事件回调handleWrite，把outputBuffer_中的数据发送出去。
    HumbleServer::Buffer inputBuffer_;

    std::shared_ptr<HumbleServer::Socket> socket_; //SocketFd
    std::shared_ptr<HumbleServer::Channel> channel_; //Channel

    EventLoop* ownerLoop_; //所属EventLoop

    EventCallback onMessageCallback_; //收到数据回调
    EventCallback onWriteCompleteCallback_; //发送完成回调
    EventCallback onCloseCallback_; //连接关闭回调
    EventCallback onErrorCallback_; //连接错误回调
};

}

/* 函数实现分割线---------------------------------------------------------------------------------------------------------------------------*/
/**
* @brief TcpConnection 构造函数，保存Loop，新建Channel，并设置其关注事件和处理函数
* @param loop 所属EventLoop
* @param SocketFd SocketFd
*/
TcpConnection::TcpConnection(EventLoop* loop, int SocketFd)
{
    //设置所属EventLoop
    ownerLoop_ = loop;

    //创建socket和Channel的实例，后续可以改为unique_ptr
    socket_ = std::make_shared<HumbleServer::Socket>(SocketFd);
    channel_ = std::make_shared<HumbleServer::Channel>(ownerLoop_, SocketFd);

    //设置Channel关注事件和处理函数
    Channel_->setEventHandler(EPOLLIN, std::bind(&TcpConnection::handleRead, this, std::placeholders::_1)); //读取客户端数据
    Channel_->setEventHandler(EPOLLOUT, std::bind(&TcpConnection::handleWrite, this, std::placeholders::_1));//向客户端发送数据
    Channel_->setEventHandler(EPOLLHUP, std::bind(&TcpConnection::handleClose, this, std::placeholders::_1));//客户端关闭连接
    Channel_->setEventHandler(EPOLLERR, std::bind(&TcpConnection::handleError, this, std::placeholders::_1));//一切错误处理，包括epoll错误或者其他
}

/**
* @brief TcpConnection 析构函数，释放内存，暂时不需要释放Fd和Channel，也没想好还有哪些需要手动释放的资源
*/
TcpConnection::~TcpConnection()
{   
    //Socket析构会调用关闭Fd，所以Fd不需要在此释放
    //Channel析构会调用update接口从epoll中移除，所以Channel也不需要在此释放
    //先调用当前类的析构释放内存以后，Tcpconnection对Socket和Channel智能指针的引用计数减一。等后续成员变量析构的时候，Socket和Channel的引用计数减一，应该就归零了，然后析构。
    handleClose(nullptr, nullptr);
}

/**
* @brief send 给上层主动调用的发送数据接口，当message长度大于系统Tcp缓冲区能存的数据量时，write返回值小于message长度，此时需要把message放到outputBuffer_中，然后注册EPOLLOUT事件，等待系统TCP缓冲区满以后触发EPOLLOUT事件回调handleWrite，把outputBuffer_中的数据发送出去。
* @param message 要发送的数据
*/
int TcpConnection::send(const std::string& message)
{
    if(ownerLoop_ == nullptr)
    {
        return;
    }
    if(ownerLoop_->isInLoopThread() != true) //当前TcpConnection如果不在自己所属Loop的线程中，则等待回到自己所属线程再调用send
    {
        ownerLoop_->runInLoop(std::bind(&TcpConnection::send, this, &message));
        return;
    }

    int messageLen = message.size();
    int sendBytes = ::write(socket_->getFd(), message.c_str(), messageLen);
    if(sendBytes == messageLen) //发送完毕
    {
        if(onWriteCompleteCallback_)
        {
            onWriteCompleteCallback_(this);
        }
    }
    else if(sendBytes >= 0
     || (sendBytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))) //发送部分数据 或者 发送缓冲区满
    {
        outputBuffer_.append(message.c_str() + sendBytes, messageLen - sendBytes); //把剩余数据放到outputBuffer_中

        if((channel_->getFocusEvent() & EPOLLOUT) == 0) //如果没有关注EPOLLOUT事件，则注册EPOLLOUT事件
        {
            channel_->setFocusEvent(EPOLLIN | EPOLLOUT);
            channel_->updateInEventLoop(EPOLL_CTL_ADD);
        }
    }
    else   //sendBytes == -1 有异常错误已经无法发送数据
    {
        //可以打印errno，可以等之后异步双缓冲日志做出来再去实现
        return Failed;
    }
}

/**
* @brief sendFile 发送文件，暂不实现，当前接口很有用，先预留在此，有一个sendFile的系统接口，可以直接发送文件内容。这样不需要调用send，避免数据多次拷贝。
    正常读文件需要先read出来，再write，这就包括了磁盘->内核缓冲区(内核态)->用户缓冲区(用户态)->内核缓冲区(内核态)->内核套接字Socket缓冲区(内核态)->网络；第一次和第四次拷贝是DMA拷贝，第二次和第三次是CPU拷贝。
    旧版sendFile：  磁盘->内核缓冲区->内核套接字Socket缓冲区->网络；第一次和第三次拷贝是DMA拷贝，第二次是CPU拷贝。
    新版sendFile：  磁盘->内核缓冲区->网络；两次都是DMA拷贝，零次CPU拷贝。
* @param filename 要发送的文件路径
* @param offset 文件偏移量，第一次传值传0
*/
int TcpConnection::sendFile(const std::string& filename, off_t offset)
{
    if(ownerLoop_ == nullptr)
    {
        return;
    }
    if(ownerLoop_->isInLoopThread() != true) //当前TcpConnection如果不在自己所属Loop的线程中，则等待回到自己所属线程再调用send
    {
        ownerLoop_->runInLoop(std::bind(&TcpConnection::sendFile, this, &filename));
        return;
    }

    int fileFd = ::open(filename.c_str(), O_RDONLY);
    if(fileFd == -1)
    {
        return Failed;
    }
    struct stat fileStatus;
    if(::fstat(fileFd, &fileStatus) == -1)
    {
        ::close(fileFd);
        return Failed;
    }
    l
    ssize_t sendFileBytes = ::sendfile(socket_->getFd(), fileFd, nullptr, 0);


}

/**
* @brief handleRead 处理数据接收，把数据放到inputBuffer_中，然后调用上层回调函数，onMessageCallback_。
         实际上，上层读取数据和网络库最重要，也是唯一的业务交互，都是通过onMessageCallback_来完成的。
         例如，在聊天服务器里面，就是直接将read出来的数据回调去一个json解析的函数，然后根据json解析出来的数据，调用对应的业务函数。
* @param now 暂时无用
*/
int TcpConnection::handleRead(TimeStamp now)
{
    
}

/**
* @brief handleWrite 处理数据发送，send发现系统TCP缓冲区满后，会触发EPOLLOUT事件回调handleWrite被动发送
* @param now 暂时无用
*/
int TcpConnection::handleWrite(TimeStamp now)
{
    
}

/**
* @brief handleClose 处理连接关闭，暂不实现
* @param now 暂时无用
*/
int TcpConnection::handleClose(TimeStamp now)
{

}

/**
* @brief handleError 处理连接错误，暂不实现
* @param now 暂时无用
*/
int TcpConnection::handleError(TimeStamp now)
{
    handleClose(arg, timePtr);
}

/**
* @brief connectFinishCallBack 连接建立后，baseLoop执行的回调。主要是TcpConnection对象创建后，在TcpSever里面设置好回调以后，再通过该callback使能Read事件，开始接收数据。
*/
int TcpConnection::connectFinishCallBack()
{

}