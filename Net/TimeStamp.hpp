#pragma once

#include <ctime>
#include <string>
#include <chrono>

namespace HumbleServer{

class TimeStamp{
//函数定义
public:
    TimeStamp();
    TimeStamp(time_t tmpTime):time_(tmpTime){};
    ~TimeStamp() = default;

    std::string getTimeString();
    time_t getTime() const {return time_;};
    time_t updateNowTime();
    void setTime(time_t tmpTime){time_ = tmpTime;};

//变量定义
public:
    time_t time_;
};

}

/* 函数实现分割线---------------------------------------------------------------------------------------------------------------------------*/

/**
    * @brief 空入参构造函数，初始化当前时间戳
*/
HumbleServer::TimeStamp()
{
    // 获取当前时间点
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    // 将时间点转换为时间戳（秒）
    std::time_t timestamp = std::chrono::system_clock::to_time_t(now);
    
    time_ = timestamp;
}

/**
    * @brief 获取当前时间的标准日期格式字符串
    * param[in] 无
    * return 返回当前时间对应的标准日期格式字符串
*/
std::string HumbleServer::TimeStamp::getTimeString()
{
    std::tm* localTime = std::localtime(&time_);
    char timeStr[66];
    std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", localTime);
    return std::string(timeStr);
}

/**
    * @brief 更新当前时间戳为现在时间
    * param[in] 无
    * return 返回当前时间对应的时间戳
*/
time_t HumbleServer::TimeStamp::updateNowTime()
{
    // 获取当前时间点
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    // 将时间点转换为时间戳（秒）
    std::time_t timestamp = std::chrono::system_clock::to_time_t(now);
    
    time_ = timestamp;
    return timestamp;
}