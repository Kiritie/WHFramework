#include "Procedure/Tests/ProcedureGraphTestTypes.h"

#include "Procedure/ProcedureModule.h"

int32 UProcedureGraphTestNode::Initialized = 0;
int32 UProcedureGraphTestNode::Entered = 0;
int32 UProcedureGraphTestNode::Shutdown = 0;

void UProcedureGraphTestNode::OnInitialize()
{
    ++Initialized;
    if (bStopOnInitialize)
    {
        GetProcedureModule()->StopMain();
    }
}

void UProcedureGraphTestNode::OnEnter(UProcedureBase* Previous)
{
    ProcedureState = EProcedureState::Entered;
    ++Entered;
    if (bStopOnEnter)
    {
        GetProcedureModule()->StopMain();
    }
    if (bAdvanceOnEnter)
    {
        Advance();
    }
}

void UProcedureGraphTestNode::OnRefresh()
{
}

void UProcedureGraphTestNode::OnLeave(UProcedureBase* Next)
{
    ProcedureState = EProcedureState::Leaved;
    if (bStopOnLeave)
    {
        GetProcedureModule()->StopMain();
    }
}

void UProcedureGraphTestNode::OnShutdown()
{
    ++Shutdown;
}
