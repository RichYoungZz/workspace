#pragma once

#include <string>

#include "public.h"

namespace HumbleServer {

/**该字符串只是作为往外发送和往内接收数据的缓冲区
*  1.每个TcpConnection对象对应一个fd的连接，而每个TcpConnection对象对应两个Buffer对象，分别对应读和写。
*  2.这个缓冲区的读和写并非对fd的数据读写，而是对当前缓冲区的vector进行写入和读取。
*  3.其实是有先后关系的，先往缓冲区里面写，缓冲区里面有数据了，才能读取。
*/
class Buffer {
//函数
public:
    Buffer();
    ~Buffer() = default;
    int writeToFd(int fd); //将缓冲区数据往fd里面发送
    int readFromFd(int fd); //从fd里面读取
    void append(const char* data, size_t len); //往缓冲区里面添加数据，与readFromFd配合使用，这里面会集成扩容策略
    int clearBuffer(); //清空缓冲区

//变量
public:
    std::vector<char> buffer_;
    //每次更新变量，直接用就好，不需要调用函数计算
    size_t readStartIndex_; // [readStartIndex_, writeEndIndex_)是有效数据，待取出来待发送的，左闭右开
    size_t writeStartIndex_; // writeStartIndex_和buffer_.size()这之间的部分是空闲的
    /**
    *   |--------------------------| *************************|
    *   | 
    *  readStartIndex_          writeStartIndex_          buffer_.size()
    * ----为有效数据，待取出来待发送的，****也就是writeStartIndex_和buffer_.size()之间的部分是空闲的，可以随意填充
    */
}

}

/* 函数实现分割线---------------------------------------------------------------------------------------------------------------------------*/

/**
* @brief 构造函数
*/
HumbleServer::Buffer::Buffer() {
    readStartIndex_ = 0;
    writeStartIndex_ = 0;
    buffer_.resize(1024); //初始化1024B = 1KB的空间先，后续会扩容，但是没有缩容策略，避免频繁分配内存，但是也会带来一点点内存浪费
}

/**
* @brief 将缓冲区数据往fd里面发送
* @param fd 
* @return -1:失败，>0:write出的字节数
*/
int HumbleServer::Buffer::writeToFd(int fd) {
    size_t writeLen = ::write(fd, buffer_.begin() + readStartIndex_, readEndIndex_ - readStartIndex_);
    if(writeLen <= 0) {
        printf("write error, writeLen = %d\n", writeLen);
    }
    ///清除已经发送的数据，当然实际上只是更新游标
    if(writeLen > 0) {
        readStartIndex_ += writeLen;
        ///游标相等说明数据已经全部发送完毕
        if(readStartIndex_ >= writeStartIndex_) 
        {
            readStartIndex_ = 0;
            writeStartIndex_ = 0;
        }
    }
    return writeLen;
}

/**
* @brief 从fd里面读取数据到缓冲区里面
* @param fd 
* @return -1:失败，>0:read出的字节数
*/
int HumbleServer::Buffer::readFromFd(int fd) {
    char exBuffer[65536]; //额外开辟一个64KB的空间，防止一次读不完。用readv代替read减少系统调用
    struct iovec iov[2];

    iov[0].iov_base = buffer_.begin() + writeStartIndex_; //缓冲区
    iov[0].iov_len = buffer_.size() - writeStartIndex_; //缓冲区剩余空间
    iov[1].iov_base = exBuffer; //额外开辟的空间
    iov[1].iov_len = sizeof(exBuffer); //额外开辟的空间大小

    int iovcnt = iov[0].iov_len < iov[1].iov_len ? 2 : 1; //经验值，通常来说一次读入会有多个TCP包，TCP单个包基本在1.5KB，最大也不会超过64KB。readv可能会读到多个TCP包，但是经验值来说，用于网络传输的也不会超过64KB
    size_t readLen = ::readv(fd, iov, iovcnt); //readv一次读入多个TCP包，减少系统调用
    if(readLen <= 0) {
        printf("read error, readLen = %d\n", readLen);
    }

    if(n <= iov[0].iov_len) { //readv已经读入数据，不需要手动append
        writeStartIndex_ += n;
    }
    else //读入到exBuffer的数据需要我们手动append
    {
       append(exBuffer, n - iov[0].iov_len);
    }

    return readLen;
}

/**
* @brief 往缓冲区里面添加数据，与readFromFd配合使用，这里面会集成扩容策略
* @param data 数据的首地址 
* @param len 数据的长度
* @return void
*/
void HumbleServer::Buffer::append(const char* data, size_t len) {
    if(len <= 0 || data == nullptr) return;
    if(len + writeStartIndex_ <= buffer_.size()) {
        ///memcpy只是单纯复制字节，copy函数会调用构造和析构，可以让资源有完整生命周期，当然char的话都无所谓
        //std::copy(data, data + len, buffer_.begin() + writeStartIndex_);
        memcpy(buffer_.begin() + writeStartIndex_, data, len);
        writeStartIndex_ += len; //更新游标
    }
    else {
        ///扩容
        buffer_.resize(writeStartIndex_ + len); //扩容
        memcpy(buffer_.begin() + writeStartIndex_, data, len);
        writeStartIndex_ += len; //更新游标
    }
}

/**
* @brief 清空缓冲区，实际上就是更新游标
* @return int 清空的字节数
*/
int HumbleServer::Buffer::clearBuffer() {
    int len = writeEndIndex_ - readStartIndex_;
    readStartIndex_ = 0;
    writeStartIndex_ = 0;
    return len;
}