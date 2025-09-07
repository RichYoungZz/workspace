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
    int sendFile(int fileFd, off_t offset, int fileLen); //发送文件 //暂不实现，当前接口很有用，先预留在此，有一个sendFile的系统接口，可以直接发送文件内容。这样不需要调用send，避免数据多次拷贝。

    int handleRead(TimeStamp now); //处理数据接收
    int handleWrite(TimeStamp now); //处理数据发送，send发现系统TCP缓冲区满后，会触发EPOLLOUT事件回调handleWrite被动发送
    int handleClose(TimeStamp now); //处理连接关闭 //暂不实现
    int handleError(TimeStamp now); //处理连接错误 //暂不实现

    void setOnMessageCallback(messageCallback cb) { onMessageCallback_ = cb; } //设置收到数据回调
    void setOnWriteCompleteCallback(writeCompleteCallback cb) { onWriteCompleteCallback_ = cb; } //设置发送完成回调
    void setOnCloseCallback(closeCallback cb) { onCloseCallback_ = cb; } //设置连接关闭回调
    void setOnErrorCallback(errorCallback cb) { onErrorCallback_ = cb; } //设置连接错误回调
    void setOnHighWaterMarkCallback(highWaterMarkCallback cb) { onHighWaterMarkCallback_ = cb; } //设置高水位回调

    int connectFinishCallBack(); //连接建立后，baseLoop执行的回调。主要是TcpConnection对象创建后，在TcpSever里面设置好回调以后，再通过该callback使能Read事件，开始接收数据。

    int getFd() const { return socket_->getFd(); } //获取SocketFd，以此来唯一标识一个TcpConnection
//变量
private:
    HumbleServer::Buffer outputBuffer_; //发送缓冲区，send调用满了以后，会放到这里且注册EPOLLOUT，等待系统TCP缓冲区满以后触发EPOLLOUT事件回调handleWrite，把outputBuffer_中的数据发送出去。
    HumbleServer::Buffer inputBuffer_;

    std::shared_ptr<Socket> socket_; //SocketFd
    std::shared_ptr<Channel> channel_; //Channel

    EventLoop* ownerLoop_; //所属EventLoop

    messageCallback onMessageCallback_; //收到数据回调
    writeCompleteCallback onWriteCompleteCallback_; //发送完成回调
    closeCallback onCloseCallback_; //连接关闭回调
    errorCallback onErrorCallback_; //连接错误回调
    highWaterMarkCallback   onHighWaterMarkCallback_; //高水位回调
};

}

/* 函数实现分割线---------------------------------------------------------------------------------------------------------------------------*/
/**
* @brief TcpConnection 构造函数，保存Loop，新建Channel，并设置其关注事件和处理函数
* @param loop 所属EventLoop
* @param SocketFd SocketFd
*/
HumbleServer::TcpConnection::TcpConnection(EventLoop* loop, int SocketFd)
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
HumbleServer::TcpConnection::~TcpConnection()
{   
    //Socket析构会调用关闭Fd，所以Fd不需要在此释放
    //Channel析构会调用update接口从epoll中移除，所以Channel也不需要在此释放
    //先调用当前类的析构释放内存以后，Tcpconnection对Socket和Channel智能指针的引用计数减一。等后续成员变量析构的时候，Socket和Channel的引用计数减一，应该就归零了，然后析构。
    //TcpConnection生命周期真正结束，则是在调用handleClose的时候，TcpConnection将从TcpServer中移除，等待loop中也执行完TcpConnection的函数以后，引用计数就没了就会被清空。
    printf("TcpConnection::~TcpConnection()\n");
}

/**
* @brief send 给上层主动调用的发送数据接口，当message长度大于系统Tcp缓冲区能存的数据量时，write返回值小于message长度，此时需要把message放到outputBuffer_中，然后注册EPOLLOUT事件，等待系统TCP缓冲区满以后触发EPOLLOUT事件回调handleWrite，把outputBuffer_中的数据发送出去。
* @param message 要发送的数据
*/
int HumbleServer::TcpConnection::send(const std::string& message)
{
    if(ownerLoop_ == nullptr)
    {
        return;
    }
    if(ownerLoop_->isInLoopThread() != true) //当前TcpConnection如果不在自己所属Loop的线程中，则等待回到自己所属线程再调用send
    {
        //值得注意的是，这里包括所有的bind，虽然函数入参是裸指针this，但是bind可以正确处理智能指针作为入参替代this裸指针，在回调执行的过程中shared_ptr作用始终有效
        ownerLoop_->runInLoop(std::bind(&TcpConnection::send, shared_from_this(), &message));//send耗时不大，可以立即执行
        return;
    }

    int messageLen = message.size();
    int sendBytes = ::write(socket_->getFd(), message.c_str(), messageLen);
    if(sendBytes == messageLen) //发送完毕
    {
        if(onWriteCompleteCallback_)
        {
            /*
            这里为什么是queueInLoop很有讲究。
            onWriteCompleteCallback_和messageCallback_都是上次注册下来的业务，muduo要求这两个函数里面避免耗时和阻塞操作，以防影响事件循环中其他Fd
            但是为什么onWriteCompleteCallback_是queueInLoop，而messageCallback_是直接调用呢？
            1.onWriteCompleteCallback_是写操作回调，里面可能还会调用write发送数据，这样就导致了频繁写导致循环阻塞。放在任务队列里面，可以将每次写事件独立开，解决Turing-complete问题。
            2.分离开写事件，也便于管理Buffer类的对象。
            3.messageCallback_里面通常是把读出的数据转到业务层了，不会有写操作，况且再read也read不到数据了，不会像写事件可能递归调用write霸占缓冲区，所以不需要独立开来。
            */
            ownerLoop_->queueInLoop(std::bind(onWriteCompleteCallback_, shared_from_this()));
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
    
    return Success;
}

/**
* @brief sendFile 发送文件，暂不实现，当前接口很有用，先预留在此，有一个sendFile的系统接口，可以直接发送文件内容。这样不需要调用send，避免数据多次拷贝。
    正常读文件需要先read出来，再write，这就包括了磁盘->内核缓冲区(内核态)->用户缓冲区(用户态)->内核缓冲区(内核态)->内核套接字Socket缓冲区(内核态)->网络；第一次和第四次拷贝是DMA拷贝，第二次和第三次是CPU拷贝。
    旧版sendFile：  磁盘->内核缓冲区->内核套接字Socket缓冲区->网络；第一次和第三次拷贝是DMA拷贝，第二次是CPU拷贝。
    新版sendFile：  磁盘->内核缓冲区->网络；两次都是DMA拷贝，零次CPU拷贝。

    解析文件路径，获取文件fd、文件大小、关闭文件fd这种属于业务操作，不在网络层处理
* @param fileFd 文件描述符
* @param offset 当前文件偏移量
* @param fileLen 从文件offset开始，需要发送的文件长度
*/
int HumbleServer::TcpConnection::sendFile(int fileFd, off_t offset, int fileLen)
{
    if(ownerLoop_ == nullptr)
    {
        return;
    }
    if(ownerLoop_->isInLoopThread() != true) //当前TcpConnection如果不在自己所属Loop的线程中，则等待回到自己所属线程再调用send
    {
        ownerLoop_->runInLoop(std::bind(&TcpConnection::sendFile, shared_from_this(), fileFd, offset, fileLen));
        return;
    }

    ssize_t sendFileBytes = ::sendfile(socket_->getFd(), fileFd, offset, 0);
    if(sendFileBytes == fileLen) //发送完毕
    {
        if(onWriteCompleteCallback_)
        {
            //可以参考send函数的注释
            ownerLoop_->queueInLoop(std::bind(onWriteCompleteCallback_, shared_from_this()));
        }
    }
    else if(sendFileBytes >= 0
    ||  (sendFileBytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))) //发送部分数据 或者 发送缓冲区满
    {
        ///剩余部分注册到Loop，由Loop继续调用sendfile发送
        ownerLoop_->queueInLoop(std::bind(&TcpConnection::sendFile, shared_from_this(), fileFd, offset, fileLen - sendFileBytes)); //offset会被sendfile更新，我们只需更新fileLen即可
    }
    else   //sendBytes == -1 有异常错误已经无法发送数据
    {
        //可以打印errno，可以等之后异步双缓冲日志做出来再去实现
        return Failed;
    }
    
    return Success;
}

/**
* @brief handleRead 处理数据接收，把数据放到inputBuffer_中，然后调用上层回调函数，onMessageCallback_。
         实际上，上层读取数据和网络库最重要，也是唯一的业务交互，都是通过onMessageCallback_来完成的。
         例如，在聊天服务器里面，就是直接将read出来的数据回调去一个json解析的函数，然后根据json解析出来的数据，调用对应的业务函数。
* @param now 暂时无用
*/
int HumbleServer::TcpConnection::handleRead(TimeStamp now)
{
    int ret = Success;
    int readBytes = inputBuffer_.readFromFd(socket_->getFd());
    if(readBytes > 0)
    {
        if(onMessageCallback_)
        {
            //这里为什么是直接调用callback而不是queuInLoop，可以去看handleWrite的注释
            onMessageCallback_(shared_from_this(), &inputBuffer_, now);
            /*
            一次readv系统调用可能读取到：
            客户端单个请求的完整数据
            单个请求的部分数据（半包）
            多个请求的合并数据（粘包）
            某个请求的后半部分+新请求的前半部分

            需要在onMessageCallback_里面处理粘包问题。可以自行在业务层和客户端协定好消息的数据头，然后通过业务层去做封装和解析！！！
            */
        }
    }
    else if(readBytes == 0) //对方关闭连接
    {
        //后续异步双缓冲修改
        printf("read 0 bytes, fd = %d, client close\n", socket_->getFd());
        handleClose(now);
    }
    else
    {
        //后续异步双缓冲修改
        printf("read bytes < 0 error, fd = %d, errno = %d\n", socket_->getFd(),  errno);
        handleError(now);
    }
    return ret;
}

/**
* @brief handleWrite 处理数据发送，send发现系统TCP缓冲区满后，会触发EPOLLOUT事件回调handleWrite被动发送
* @param now 暂时无用
*/
int HumbleServer::TcpConnection::handleWrite(TimeStamp now)
{
    int ret = Success;
    if(outputBuffer_.readableBytes() <= 0)
    {
        return ret;
    }

    int writeBytes = outputBuffer_.writeToFd(socket_->getFd()); //writeToFd会返回已发送次数并且重制可读和可写的标志位，不需要另外的retrieve去重置
    if(outputBuffer_.readableBytes() == 0) //发送完毕
    {
        if(onWriteCompleteCallback_)
        {
            ownerLoop_->queueInLoop(std::bind(onWriteCompleteCallback_, shared_from_this()));
        }
        //关闭可读事件
        channel_->setFocusEvent(EPOLLIN);
        channel_->updateInEventLoop(EPOLL_CTL_MOD);
    }
    else if(writeBytes < 0)
    {
        //出错了，等后续完成异步双缓冲再去写
        printf("write error, fd = %d, errno = %d\n",  socket_->getFd(), errno);
        ret =  Failed;
    }
    return ret;
}

/**
* @brief handleClose 处理连接关闭
* @param now 在收到错误的时候手动调用，实际上就是通知自己上层TcpServer去删除对自己的持有，减少引用计数来触发析构。
             目前情况是，Channel和Socket在自己的析构函数中做释放，等到TcpConnection析构的时候，它们就会自动析构，不需要显式调用类似handleClose的函数来通知上层来释放。
             而TcpConnection是在遇到错误的时候，显式调用handleClose，来通知上层删除自己，以达到释放的目的。
*/
int HumbleServer::TcpConnection::handleClose(TimeStamp now)
{
    int ret = Success;
    printf("TcpConnection::handleClose()\n");
    if(onCloseCallback_)
    {
        onCloseCallback_(shared_from_this());
    }
    else
    {
        printf("TcpConnection::onCloseCallback_ is null\n");
        //没显式调用CloseCallback通知TcpServer，会导致该TcpConnection对象一直存在，直到TcpServer析构
        assert(0);
    }
    return ret;
}

/**
* @brief handleError 处理连接错误，暂不实现
* @param now 暂时无用
*/
int HumbleServer::TcpConnection::handleError(TimeStamp now)
{
    int ret = Success;
    ret = handleClose(arg, timePtr);
    channel->setFocusEvent(0);
    channel->updateInEventLoop(EPOLL_CTL_DEL);
    return Success;
}

/**
* @brief connectFinishCallBack 连接建立后，baseLoop执行的回调。主要是TcpConnection对象创建后，在TcpSever里面设置好回调以后，再通过该callback使能Read事件，开始接收数据。
*/
int HumbleServer::TcpConnection::connectFinishCallBack()
{
    //不能在构造函数中调用，因为回调函数等还没设置好，不能直接开始监听EPOLLIN
    int ret = Success;
    channel->setFocusEvent(EPOLLIN);
    channel->updateInEventLoop(EPOLL_CTL_ADD);
    channel->setTie(shared_from_this());
    return ret;
}