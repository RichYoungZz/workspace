#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unistd.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "public.h"
#include "Channel.hpp"
#include "EpollPoller.hpp"
#include "TimeStamp.hpp"

/**
* @brief EventLoop类，事件循环类，管理一个EpollPoller和多个Channel
1. epoll系统接口的多线程风险
线程安全风险：epoll操作（如epoll_wait和epoll_ctl）本身不是线程安全的。如果多个线程并发调用这些接口（例如，一个线程正在执行epoll_wait监控事件，另一个线程调用epoll_ctl添加或修改文件描述符），可能导致竞态条件。这包括：
事件丢失或重复：并发修改epoll实例的内部状态（如epoll的interest list）可能导致事件未被正确处理，例如新添加的fd事件被遗漏或已移除的fd事件仍被触发。
状态不一致：内核数据结构（如epoll实例）的并发访问可能导致未定义行为，包括数据损坏或程序崩溃。例如，在epoll_wait中监控一个fd时，另一个线程的epoll_ctl操作可能破坏监控状态。
根本原因：epoll系统调用依赖共享内存结构，缺乏内在同步机制。在Reactor模式中，每个I/O线程（如SubReactor）应独占其epoll实例的访问权。直接跨线程调用updateChannel（隐含调用epoll_ctl）而不通过任务队列（如runInLoop的机制），会破坏这种独占性，增加竞争风险。
*/
namespace HumbleServer{
__thread EventLoop *t_loopInThisThread = nullptr; //线程局部变量，指向当前线程的EventLoop，也可以检测一个线程是否创建EventLoop，防止创建多个
__thread int t_threadIdInThisThread = 0; //线程局部变量，记录当前线程的线程ID，用于判断当前线程是否创建了EventLoop

class EventLoop{
//函数
public:
    EventLoop();
    ~EventLoop() = default;

    void loop(); //事件循环
    int updateChannel(std::shared_ptr<Channel> channel, int cmd); //更新Channel到epoll

    int runInLoop(EventCallback cb); //在当前线程的EventLoop中执行cb
    int queueInLoop(EventCallback cb); //在当前线程的EventLoop中执行cb

    int handleRead(TimeStamp now); //处理唤醒事件 这里是被其他线程唤醒的时候，从EpollPoller的poll中跳出，执行callback_里的函数，不读的话会一直被唤醒
    int wakeup(); //唤醒EventLoop

    int  getThreadId() const { return threadId_; } //获取线程ID
    bool isInLoopThread() const { return threadId_ == t_threadIdInThisThread; } //判断当前线程是否是EventLoop所在的线程
//变量
private:
    int wakeUpFd_; //eventfd句柄 用于EventLoop间通信
    int status; //loop状态
    std::unique_ptr<EpollPoller> poller_; //epoll事件监听器
    std::unique_ptr<Channel> wakeUpFdChannel_; //eventfd对应的channel，用于唤醒
    
    std::vector<Channel*> activeChannels_; //活跃的channel 用于从EpollPoller中获取活跃的channel
    std::vector<EventCallback> callbacks_; //事件回调函数，不属于任何Channel的回调函数，处理例如主Reactor的新连接事件等

    std::mutex mutex_; //互斥锁 对于callbacks_的访问，其他线程添加，本线程处理。可以理解成读写事件，无法避免的需要加锁

    int threadId_; //线程ID，用于判断当前EventLoop是否运行在对应的线程中
};

}

/* 函数实现分割线---------------------------------------------------------------------------------------------------------------------------*/

/**
* @brief EventLoop构造函数
*/
HumbleServer::EventLoop::EventLoop()
{
    ///初始化线程局部变量
    if(t_loopInThisThread) //如果当前线程已经创建了EventLoop，直接返回
        assert(false);
    t_loopInThisThread = this;
    t_threadIdInThisThread = syscall(SYS_gettid);//std::this_thread::get_id();

    wakeUpFd_  = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC); //创建eventfd
    wakeUpFdChannel_ = std::make_unique<Channel>(std::make_shared<Socket>(wakeUpFd_)); //创建eventfd对应的channel
    wakeUpFdChannel_->setEventHandler(EPOLLIN, std::bind(&EventLoop::handleRead, this, std::placeholders::_1)); //设置eventfd对应的channel的回调函数

    poller_ = std::make_unique<EpollPoller>(this); //创建epoll事件监听器

    status = EventLoopStatus_Init;
    threadId_ = t_threadIdInThisThread;
}

/**
* @brief EventLoop事件循环
*/
void HumbleServer::EventLoop::loop()
{
    //每个EventLoop只能loop一次
    if(status == EventLoopStatus_Running)
    {
        printf("EventLoop::loop() already called, status is %d, dont called again\n", status);
        //assert(false);
        return;
    }

    std::vector<EventCallback> tempCallbacks;

    callbacks_.clear(); //清空
    tempCallbacks.clear(); //清空
    status = EventLoopStatus_Running;
    while(status == EventLoopStatus_Running)
    {
        activeChannels_.clear();
        poller_->poll(&activeChannels_, 10000); //阻塞等待事件发生，最多等待10s
        for(Channel* channel: activeChannels_)
        {
            channel->handleEvent(); //处理事件
        }
        //执行当前EventLoop所需的回调
        /*
         * mainLoop => accept => fd 产生 channel => 分发subLoop
         * mainLoop需要分发fd给subLoop，不需要subLoop从阻塞队列里面取
         * 【分三步】
         * 1.完成这个过程，需要mainLoop事先通过runInLoop注册回调到callbacks_里面
         * 2.wakeup subLoop
         * 3.subLoop来执行mainLoop注册的这个回调
         */
        {
            std::lock_guard<std::mutex> lock(mutex_);
            tempCallbacks.swap(callbacks); //清空callbacks，防止在loop中添加新的回调函数
        }
        for(EventCallback& cb: tempCallbacks)
        {
            cb(nullptr);
        }
        tempCallbacks.clear();
    }
}

/**
* @brief 更新Channel到epoll
* @param channel 需要更新的Channel
* @param cmd 操作类型 EPOLL_CTL_ADD添加 EPOLL_CTL_MOD修改  EPOLL_CTL_DEL删除
*/
int HumbleServer::EventLoop::updateChannel(std::shared_ptr<HumbleServer::Channel> channel, int cmd)
{
    poller_->updateChannel(channel, cmd);
    return Success;
}

/**
* @brief 在当前线程的EventLoop中注册回调函数
* @param cb 需要注册的回调函数
  当初代码在写到这里的时候，我有疑问。就是当queueInLoop的时候，callbacks_和poll是顺序执行的，不存在竞态条件；那当runInLoop的时候，一个线程里面有poll调用和cb()调用，为什么就没有竞态条件了呢？
  答：其实这里我走到了一个误区，一个线程内不可能并行地执行poll和cb()，在没有协程的情况下，一个线程内哪怕有多个函数调用，本质上也是顺序执行的，也就是poll里的epoll_wait和新连接加入的cb()里面的epoll_ctl一定是顺序执行的，不存在多线程问题。
*/
int HumbleServer::EventLoop::runInLoop(EventCallback cb)
{
    if(threadId_ == t_threadIdInThisThread) //说明当前线程就是EventLoop所在的线程，直接执行cb
    {
        cb();
    }
    else//说明当前线程不是EventLoop所在的线程，添加到callbacks_里面，到对应线程中再执行。也正因为不在同一个线程，所以需要加锁和唤醒
    {
        queueInLoop(cb);
    }
    return Success;
}

/*
* @brief 在当前线程的EventLoop中将cb添加到任务队列
* @param cb 需要注册的回调函数
*/
int queueInLoop(EventCallback cb)
{
    ///首先lock_guard实际上调用的也是mutex的lock和unlock，但是lock_guard的构造函数和析构函数会自动调用，所以不需要手动unlock。同时lock和unlock都是原子操作
    {
        ///这里减少一个queueInLoop的函数调用
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks_.emplace_back(std::move(cb));
    }
    wakeup(); //唤醒EventLoop
}

/**
* @brief 唤醒EventLoop处理事件，也就是向对应EventLoop的wakeUpFd_发送信号，使其从EpollPoller的poll中跳出，执行callback_里的函数
*/
int HumbleServer::EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = ::write(wakeUpFd_, &one, sizeof(one)); //向wakeUpFd_写入一个字节
    if(n != sizeof(one))
    {
        printf("EventLoop::wakeup() writes %lld bytes instead of 8, error\n", n);
    }
    return Success;
}

/**
* @brief 处理唤醒事件，被唤醒以后及时读出数据，防止被多次唤醒
* @param receiveTime 事件发生的时间
*/
int HumbleServer::EventLoop::handleRead(TimeStamp now)
{
    uint64_t one = 1;
    ssize_t n = ::read(wakeUpFd_, &one, sizeof(one)); //n代表读到的字节数
    if(n != sizeof(one))
    {
        printf("EventLoop::handleRead() reads %lld bytes instead of 8, error\n", n);
        assert(false);
    }
    return Success;
}