#pragma once

#include <functional>
#include <memory>
#include <unordered_map>

#include "public.h"
#include "Socket.hpp"
#include "TimeStamp.hpp"
#include "EventLoop.hpp"

namespace HumbleServer{

class EventLoop;

/**
@brief 事件处理类，负责将Fd与读、写、错误处理等事件的处理函关联起来。由EventLoop来管理 【功能类/变量类】对于普通fd，它有各种处理事件，像是功能。对于EventLoop来说更像一个变量而已
*/
class Channel {
//函数定义
public:
    Channel(std::shared_ptr<Socket> socket, EventLoop * loop):  socket_(socket), 
                                                                ownerLoop_(loop),
                                                                focusEvent_(0), 
                                                                needToHandleEvent_(0), 
                                                                tie_(nullptr)
    {

    }; //必须有对应的Socket才能创建Channel，创建时候不用传入EventLoop，创建好以后再后续分配 //20250819:发现进来时候直接赋值loop也可以
    ~Channel();

    /* 获取一些变量*/
    int getFd() const {return socket_->getFd();};
    int getFocusEvent() const {return focusEvent_;};
    int getNeedToHandleEvent() const {return needToHandleEvent_;};

    /* 设置一些变量*/
    void setFocusEvent(int event)  {focusEvent_ = event;}; //设置该Channel关注事件
    void setNeedToHandleEvent(int event) {needToHandleEvent_ = event;}; // 由EpollPoller设置该Channel实际发生事件
    void setEventHandler(int eventType, EventCallbackWithTimeStamp eventHandler) {eventHandlerMap_[eventType] = std::move(eventHandler);}; //设置事件处理函数
    void setEventLoop(EventLoop * loop) {ownerLoop_ = loop;}; //设置所属EventLoop
    void setTie(std::shared_ptr<void> tie) {tie_ = tie;}; //设置tie_

    /* 真正处理事件*/
    FunctionResultType handleEvent();
    /* 更新EventLoop对应事件*/
    int updateInEventLoop(int cmd);

//变量定义
private:
    std::shared_ptr<Socket> socket_; //取代掉fd，不然还需要手动释放，由Acceptor传进来，Channel不负责创建，所以交给智能指针来管理
    int focusEvent_; //关注的事件，需要epoll监听，而非实际发生的事件
    int needToHandleEvent_; //epoll返回来需要真正处理的事件

    EventLoop * ownerLoop_; //当前Channel所属的EventLoop
    std::unordered_map<int, EventCallbackWithTimeStamp > eventHandlerMap_; //事件处理函数map

    std::weak_ptr<void> tie_; //用于观测Channel的生命周期
};

} 

/* 函数实现分割线---------------------------------------------------------------------------------------------------------------------------*/

/**
    * @brief 处理事件
    * param[in] arg 事件处理函数参数
    * param[in] timeStamp* 时间戳指针，有时候为nullprt
    * return 返回处理结果
*/
HumbleServer::FunctionResultType HumbleServer::Channel::handleEvent()
{
    TimeStamp now;
    FunctionResultType ret = Success;
    auto it = tie_.lock();
    if(it == nullptr)
    {
        //如果weak_ptr已经失效，说明Channel已经析构，直接返回
        return ret;
    }

    if(needToHandleEvent_ & EPOLLIN && eventHandlerMap_.find(EPOLLIN) != eventHandlerMap_.end())
    {
        eventHandlerMap_[EPOLLIN](now);
    }
    if(needToHandleEvent_ & EPOLLOUT && eventHandlerMap_.find(EPOLLOUT) != eventHandlerMap_.end())
    {
         eventHandlerMap_[EPOLLOUT](now);
    }
    if(needToHandleEvent_ & EPOLLERR && eventHandlerMap_.find(EPOLLERR) != eventHandlerMap_.end())
    {
         eventHandlerMap_[EPOLLERR](now);
    }
    if(needToHandleEvent_ & EPOLLHUP && eventHandlerMap_.find(EPOLLHUP) != eventHandlerMap_.end())
    {
         eventHandlerMap_[EPOLLHUP](now);
    }
    return ret;
}

/**
    * @brief 更新EventLoop对应事件，调用所属的EventLoop，让ownLoop去通知loop拥有的EpollPoller去更新
    * @return 0成功，非0失败
*/
int HumbleServer::Channel::updateInEventLoop(int cmd)
{
    if(ownerLoop_ != nullptr)
    {
        int ret = ownerLoop_->updateChannel(std::shared_from_this(), cmd);
        if(ret != Success)
        {
            printf("updateEventLoop error: %d\n", ret);
            return Failed;
        }
    }
    else
    {
        printf("updateEventLoop error: ownerLoop_ is nullptr\n");
        return Failed;
    }
    return Success;
}

HumbleServer::Channel::~Channel()
{
    setFocusEvent(0);
    setNeedToHandleEvent(0);
    updateInEventLoop(EPOLL_CTL_DEL);
}