#pragma once

#include <functional>
#include <memory>
#include <unordered_map>

#include "public.h"
#include "Socket.hpp"
#include "TimeStamp.hpp"

namespace HumbleServer{

class EventLoop;

class Channel {
//函数定义
public:
    Channel(std::shared_ptr<Socket> socket):socket_(socket), focusEvent_(0), needToHandleEvent_(0){}; //必须有对应的Socket才能创建Channel
    ~Channel() = default;

    /* 获取一些变量*/
    int getFd() const {return socket_->getFd();};
    int getFocusEvent() const {return focusEvent_;};
    int getNeedToHandleEvent() const {return needToHandleEvent_;};

    /* 设置一些变量*/
    void setFocusEvent(int event) {focusEvent_ = event;}; //设置该Channel关注事件
    void setNeedToHandleEvent(int event) {needToHandleEvent_ = event;}; //由EventLoop设置该Channel实际发生事件
    void setEventHandler(EventType eventType, EventCallbackWithTimeStamp eventHandler) {eventHandlerMap_[eventType] = std::move(eventHandler);}; //设置事件处理函数

    /* 真正处理事件*/
    FunctionResultType handleEvent(void* arg, TimeStamp timeStamp);
    /* 更新EventLoop对应事件*/
    void updateEventLoop();

//变量定义
public:
    std::shared_ptr<Socket> socket_; //取代掉fd，不然还需要手动释放
    int focusEvent_; //关注的事件，需要epoll监听，而非实际发生的事件
    int needToHandleEvent_; //epoll返回来需要真正处理的事件

    EventLoop * ownerLoop_; //当前Channel所属的EventLoop
    std::unordered_map<EventType, EventCallbackWithTimeStamp > eventHandlerMap_; //事件处理函数map

    std::weak_ptr<Channel> tie_; //用于观测Channel的生命周期
};

} 

/* 函数实现分割线---------------------------------------------------------------------------------------------------------------------------*/

/**
    * @brief 处理事件
    * param[in] arg 事件处理函数参数
    * param[in] timeStamp 时间戳
    * return 返回处理结果
*/
HumbleServer::FunctionResultType HumbleServer::Channel::handleEvent(void* arg, TimeStamp timeStamp)
{
    FunctionResultType ret = FunctionResultType_Success;
    auto it = tie_.lock();
    if(it == nullptr)
    {
        //如果weak_ptr已经失效，说明Channel已经析构，直接返回
        return ret;
    }
    for(int i = 0; i < EventType_All; i++)
    {
        if(needToHandleEvent_ & (1 << i) && eventHandlerMap_.find(i) != eventHandlerMap_.end())
        {
            ret = eventHandlerMap_[i](arg, timeStamp);
            if(ret != FunctionResultType_Success) //先鲁棒地这么写，有一个回调执行失败直接返回。
            {
                printf("handleEvent %d error: %d\n", i, ret);
                break;
            }
        }
    }
    return ret;
}

/**
    * @brief 更新EventLoop对应事件
    * @return 0成功，非0失败
*/
void HumbleServer::Channel::updateEventLoop()
{
    if(ownerLoop_ != nullptr)
    {
        ownerLoop_->updateChannel(shared_from_this());
    }
    else
    {
        printf("updateEventLoop error: ownerLoop_ is nullptr\n");
    }
}