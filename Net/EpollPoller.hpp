#pragma once

#include <sys/epoll.h>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <string>
#include <unistd.h>
#include <signal.h>

#include "public.h"
#include "Channel.hpp"
#include "EventLoop.hpp"

namespace HumbleServer{

/**
* @brief 对epoll进行封装，监听已有的连接上有什么事件发生，由EventLoop调用。一个EpollPoller对应一个EventLoop 【功能类】
*/
class EpollPoller{
//函数
public:
    EpollPoller();
    ~EpollPoller() = default;

    int updateChannel2Epoll(std::shared_ptr<Channel> channel, int cmd); //通过channel的events_判断关注的事件，根据Channel的status来判断状态
    int epoll(ChannelList* activeChannels, int timeoutMs = 1000);
//变量
private:
    int epollFd_; //epoll句柄
    int eventListSize_; //epoll返回的事件列表大小
    std::unordered_map<int, std::shared_ptr<Channel> > channelMap_; //Channel的fd和Channel的映射，用于epoll返回后，根据fd找到对应的Channel
    std::vector<epoll_event> eventList_; //epoll返回的事件列表
};

}

/* 函数实现分割线---------------------------------------------------------------------------------------------------------------------------*/

HumbleServer::EpollPoller::EpollPoller()
{
    eventListSize_ = 16;
    channelMap_.clear();
    eventList_.resize(eventListSize_); 
    epollFd_ = epoll_create1(EXCL_CLOEXEC); ///EXCL_CLOEXEC让进程重入的时候epollFd失效，不会被继承
    if(epollFd_ < 0)
    {
        printf("epoll create error\n");
        assert(false);
    }
}

/**
 * @brief 更新Channel到epoll
 * param[in]  channel 输入参数 - Channel
 * param[in]  cmd     输入参数 - ChannelStatus 表示Channel的状态 添加/修改/删除
 * @return FunctionResultType
 */
int HumbleServer::updateChannel2Epoll(std::shared_ptr<Channel> channel, int cmd)
{
    if(channel == nullptr)
    {
        printf("channel is nullptr\n");
        return Failed;
    }
    ///Acceptor也通过传入Channel来注册
    struct epoll_event event;
    bzero(&event, sizeof(event));
    event.data.ptr = channel.get(); ///存储额外的Channel信息
    event.data.fd = channel->getFd();
    event.events = channel->getFocusEvent();

    if(channelMap_.find(event.data.fd) == channelMap_.end() && cmd != EPOLL_CTL_ADD) ///不是添加且不在map里面，说明不存在，不能操作
    {
        printf("channel %d not exist, can't update it\n", event.data.fd);
        return Failed;
    }

    if(channelMap_.find(event.data.fd) != channelMap_.end() && cmd == EPOLL_CTL_ADD) ///已经在channelMap_中，不能重复添加，更新即可
    {
        cmd = EPOLL_CTL_MOD;
        printf("channel %d already exist, update it\n", event.data.fd);
    }

    switch(cmd)
    {
        case EPOLL_CTL_ADD: ///EpollPoller新增Channel
            if(epoll_ctl(epollFd_, EPOLL_CTL_ADD, event.data.fd, &event) < 0)
            {
                printf("epoll add error\n");
                return Failed;
            }
            else
            {
                printf("epoll add success, fd = %d\n",  event.data.fd);
            }
            channelMap_[event.data.fd] = channel; ///将Channel的fd和Channel的映射关系存入channelMap_中
            break;
        case EPOLL_CTL_MOD:
            if(epoll_ctl(epollFd_, EPOLL_CTL_MOD, event.data.fd, &event) < 0)
            {
                printf("epoll mod error\n");
                return Failed;
            }
            else
            {
                printf("epoll mod success, fd = %d\n",  event.data.fd);
            }
            break;
        case EPOLL_CTL_DEL:
            if(epoll_ctl(epollFd_, EPOLL_CTL_DEL, event.data.fd, &event) < 0)
            {
                printf("epoll del error\n");
                return Failed;
            }
            else
            {
                printf("epoll del success, fd = %d\n",  event.data.fd);
            }
            channelMap_.erase(event.data.fd); ///删除channelMap_中的映射关系
            break;
        default:
            printf("epoll update error, unsupport cmd: %d\n", cmd);
            return Failed;
    }
    return Success;
}

int HumbleServer::epoll(ChannelList* activeChannels, int timeoutMs)
{
    //监听事件，返回活跃的Channel
    int eventCount = epoll_wait(epollFd_, eventList_.data(), eventListSize_, timeoutMs); //vector::data() 方法用于获取指向 vector 容器内部数组的指针
    if(ret < 0)
    {
        printf("epoll wait error\n");
        return Failed;
    }
    //将活跃的Channel存入activeChannels
    for(int i = 0; i < eventCount; i++)
    {
        Channel* channel = (Channel*)eventList_[i].data.ptr;
        if(channel == nullptr || channelMap_.find(channel->getFd()) == channelMap_.end())
        {
            printf("channel is nullptr\n");
            continue;
        }
        channel->setNeedToHandleEvent(eventList_[i].events);
        activeChannels->emplace_back(std::move(channel));
    }
    //如果活跃Channel数量等于eventListSize_，说明eventList_不够用，需要扩容，每次只扩容2倍
    if(eventCount == eventListSize_)
    {
        eventListSize_ <<= 1; 
        eventList_.resize(eventListSize_);
    }
    return Success;
}