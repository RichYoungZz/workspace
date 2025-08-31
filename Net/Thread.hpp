#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <assert.h>

#include "Socket.hpp"
#include "EventLoop.hpp"
#include "public.h"

namespace HumbleServer {

/**
* @brief 线程类，封装了线程的创建和运行，为确保One loop per thread而使用。
*/
class Thread {
//函数
public:
    Thread();
    ~Thread() = default;

    void threadMain(); //线程主函数
    EventLoop* start(); //启动线程
//变量
public:
    std::mutex mutex_;
    std::condition_variable cond_;
    std::shared_ptr<std::thread> threadPtr_; ///指向创建的线程 不能用变量，线程被创建以后就会马上执行

    EventLoop *loop_; ///指向线程的loop

    EventCallback callback_; ///线程主函数创建运行以后的回调函数 可有可无，看传不传
    pid_t tid_; ///线程id
};

}

/* 函数实现分割线---------------------------------------------------------------------------------------------------------------------------*/

HumbleServer::Thread::Thread() : loop_(nullptr), tid_(0) {
    printf("class Thread Init\n");
}

/**
* @brief 启动线程，当前函数和ThreadPool不运行在同一个线程，ThreadPool需要获取loop的话，需要条件变量
*/
void HumbleServer::Thread::threadMain() {
    printf("ThreadMain is running\n");
    EventLoop loop;

    if(callback_)
    {
        callback_(&loop);
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one(); ///加锁后，唤醒主线程，告诉主线程自己已经拿到loop了，同时跳出{}释放锁
    }
    loop.loop();
    loop_ = nullptr; ///线程退出后，loop_置空
}

/**
* @brief 启动线程，当前函数运行在主线程，等待条件变量唤醒以后，说明线程和Loop都已经创建好了，返回loop
*/
EventLoop* HumbleServer::Thread::start() {
    printf("ThreadMain is start\n");

    //std::counting_semaphore<1> sem(0); ///创建一个信号量，初始值为0，计数上限为1
    threadPtr_ = std::make_shared<std::thread>(&Thread::threadMain, this); //make_shared包括了分配内存+构造函数

    {
        std::unique_lock<std::mutex> lock(mutex_);
        while(loop_ == nullptr)
        {
            cond_.wait(lock); ///释放锁，等待唤醒，唤醒后重新加锁
        }
        //到此说明loop_已经被创建
    }
    tid_ = loop_->threadId_; ///先创建完loop_以后，再获取线程id
    printf("Thread is Inited, tid = %d\n", tid_);

    ///tid_为0，说明线程还没创建好，直接报异常assert
    if(tid_ == 0)
    {
        printf("tid is 0\n");
        assert(false);
    }
    return loop_;
}