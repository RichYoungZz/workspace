#pragma once

#include "public.h"
#include "Thread.hpp"

namespace HumbleServer{

/**
* @brief 线程池类，用于综合管理线程，主要是为了让网络库框架中每个模块都能有自己的抽象，其实作用不是很大，耦合在TcpServer中我感觉也可以【功能类】
*/
class ThreadPool{
//函数
public:
    ThreadPool(EventLoop* baseLoop, int numThreads = 0);
    ~ThreadPool() = default;
    int start();
    EventLoop* getNextLoopFromRoundRobin();

    void setThreadNum(int numThreads) { numThreads_ = numThreads; }
//变量
public:
    std::vector<std::unique_ptr<Thread> > threadsVector;
    std::vector<EventLoop*> loopsVector; //所有线程的EventLoop，EventLoop不用智能指针，因为EventLoop是局部变量，线程start函数执行完就销毁了，不用管理
    int next_; //下一个EventLoop的索引，用的round robin算法也就是轮询，后续可以更换负载均衡算法

    EventLoop* baseLoop_; //主线程的EventLoop，当没有设置线程数量的时候，默认就是主线程的EventLoop完成所有工作
    int numThreads_; //线程数量
};

}

/* 函数实现分割线---------------------------------------------------------------------------------------------------------------------------*/

/**
* @brief 构造函数，初始化，此时就可以传入numThreads和baseLoop，numThreads可以不传后续再设置，baseLoop必须传
*/
HumbleServer::ThreadPool::ThreadPool(EventLoop* baseLoop, int numThreads)
    : baseLoop_(baseLoop), numThreads_(numThreads), next_(0) {
        threadsVector.clear();
        loopsVector.clear();

        if(baseLoop_ == nullptr)
        {
            printf("baseLoop_ is nullptr\n");
            assert(false);
        }
}

/**
* @brief 获取下一个EventLoop，目前用的是round robin算法
* @return EventLoop* 指向下一个EventLoop的指针
*/
EventLoop* HumbleServer::ThreadPool::getNextLoopFromRoundRobin() {
    EventLoop* loop = baseLoop_;
    if(!loopsVector.empty())
    {
        if(next_ >= loopsVector.size())
        {
            printf("next_ >= loopsVector.size(), next = %d, loopVectorSize = %d, numThreads = %d\n", next_, loopsVector.size(), numThreads_);
            return baseLoop_;
        }
        loop = loopsVector[next_];
        next_ = (next_ + 1) % numThreads_;
    }

    return loop;
}

/**
* @brief 启动线程池，创建线程，创建EventLoop，将EventLoop放入loopsVector中
*/
int HumbleServer::ThreadPool::start() {
    if(numThreads_ <= 0)
    {
        printf("numThreads_ <= 0, numThreads = %d\n", numThreads_);
        return;
    }

    EventLoop* loop = nullptr;
    for(int i = 0; i < numThreads_; i++)
    {
        loop = nullptr;
        threadsVector.push_back(std::make_unique<Thread>());
        loop = threadsVector[i]->start(); //创建线程，线程里面会创建EventLoop，start内部保证了线程同步创建完EventLoop以后才会返回
        if(threadsVector[i] == nullptr || loop == nullptr)
        {
            printf(" threadsVector[i] == %p, loop == %p\n", threadsVector[i], loop);
            assert(false);
        }
        loopsVector.push_back(loop);
        printf("ThreadPool start success, i = %d, threads = %p, loop = %p\n", i, threadsVector[i], loop);
    }
    printf("ThreadPool start success, numThreads = %d\n", numThreads_);
    return Success;
}