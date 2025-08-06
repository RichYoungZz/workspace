#param once

#define IN IN
#define OUT OUT
#define INOUT INOUT

namespace HumbleServer{

using EventCallbackWithTimeStamp = std::function<FunctionResultType(void*, TimeStamp)>;
using EventCallback = std::function<FunctionResultType(void*)>;
using ChannelList = std::vector<Channel*>;

typedef enum EventType{
    EventType_Read = 0,
    EventType_Write = 1,
    EventType_Error = 2,
    EventType_Close = 3,
    EventType_Connect = 4,

    //记录所有事件数量，方便遍历，有需要往上添加即可
    EventType_All,
}; 

//函数返回值
typedef enum FunctionResultType{
    FunctionResultType_Success = 0,
    FunctionResultType_Fail = 1,

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