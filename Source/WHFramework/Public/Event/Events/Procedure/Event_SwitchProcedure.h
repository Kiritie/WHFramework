#pragma once

#include "Event/EventModuleTypes.h"
#include "Procedure/ProcedureModuleTypes.h"
#include "Event_SwitchProcedure.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventSwitchProcedure : public FEventBase
{
    GENERATED_BODY()

    FEventSwitchProcedure() = default;
    FEventSwitchProcedure(TSubclassOf<class UProcedureBase> InProcedureClass)
        : ProcedureClass(InProcedureClass)
    {
    }
    FEventSwitchProcedure(const FProcedureActivationToken& InToken, TSubclassOf<class UProcedureBase> InProcedureClass)
        : ProcedureClass(InProcedureClass), Token(InToken)
    {
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSubclassOf<class UProcedureBase> ProcedureClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FProcedureActivationToken Token;
};
