#param once

#define IN IN
#define OUT OUT
#define INOUT INOUT

namespace HumbleServer{

using EventCallbackWithTimeStamp = std::function<int(TimeStamp)>;
using EventCallback = std::function<int()>;
using ChannelList = std::vector<Channel*>;

//函数返回值
typedef enum FunctionResultType{
    Success = 0,
    Fail = 1,

    //记录所有结果数量
    FunctionResultType_All,
};

}

//Channel状态，会影响到EpollPoller中是否删除
typedef enum ChannelStatus{
    ChannelStatus_None = 0,
    ChannelStatus_Added = 1,
    ChannelStatus_Deleted = 2,
    ChannelStatus_Modified = 3,
    ChannelStatus_Error = 4,

    //记录所有状态数量
    ChannelStatus_All,
}

//EventLoop状态
typedef enum EventLoopStatus{
    EventLoopStatus_None = 0,
    EventLoopStatus_Running = 1,
    EventLoopStatus_Stoped = 2,
    EventLoopStatus_Error = 3,
    EventLoopStatus_Init = 4,

    //记录所有状态数量
    EventLoopStatus_All,
}