#pragma once

#include "Procedure/Base/ProcedureBase.h"
#include "ProcedureGraphTestTypes.generated.h"

UCLASS()
class UProcedureGraphTestNode : public UProcedureBase
{
    GENERATED_BODY()

public:
    static int32 Initialized;
    static int32 Entered;
    static int32 Shutdown;
    UPROPERTY()
    bool bAdvanceOnEnter = false;
    UPROPERTY()
    bool bStopOnLeave = false;
    UPROPERTY()
    bool bStopOnEnter = false;
    UPROPERTY()
    bool bStopOnInitialize = false;

    virtual void OnInitialize() override;
    virtual void OnEnter(UProcedureBase* Previous) override;
    virtual void OnRefresh() override;
    virtual void OnLeave(UProcedureBase* Next) override;
    virtual void OnShutdown() override;
};

UCLASS()
class UProcedureGraphSecondTestNode : public UProcedureGraphTestNode
{
    GENERATED_BODY()
};

UCLASS()
class UProcedureGraphThirdTestNode : public UProcedureGraphTestNode
{
    GENERATED_BODY()
};

UCLASS()
class UProcedureGraphDetachedTestNode : public UProcedureGraphTestNode
{
    GENERATED_BODY()
};
