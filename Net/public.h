#param once

#define IN IN
#define OUT OUT
#define INOUT INOUT

namespace HumbleServer{

using EventCallbackWithTimeStamp = std::function<FunctionResultType(void*, TimeStamp)>;
using EventCallback = std::function<FunctionResultType(void*)>;

typedef enum EventType{
    EventType_Read = 0,
    EventType_Write = 1,
    EventType_Error = 2,
    EventType_Close = 3,
    EventType_Connect = 4,

    //记录所有事件数量，方便遍历，有需要往上添加即可
    EventType_All,
}; 

typedef enum FunctionResultType{
    FunctionResultType_Success = 0,
    FunctionResultType_Fail = 1,

    //记录所有结果数量
    FunctionResultType_All,
};

}
