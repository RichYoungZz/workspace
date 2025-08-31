#pragma once

namespace HumbleServer{

/**
* @brief TcpServer  服务器类，持有一个Acceptor监听链接，多个EventLoop管理EpollPoller和Channel，去对每个fd进行处理。还持有若干TcpConnection一一对应Channel。【功能类】
*/
class TcpServer{
//函数
public:
//变量
public:
};

}