#pragma once

#include "CoreMinimal.h"
#include "ProcedureModuleTypes.generated.h"

UENUM(BlueprintType)
enum class EProcedureState : uint8
{
    None,
    Entered,
    Leaved
};

UENUM(BlueprintType)
enum class EProcedureGuideType : uint8
{
    None,
    TimerOnce,
    TimerLoop
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FProcedureActivationToken
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FGuid NodeId;

    UPROPERTY(BlueprintReadOnly)
    int64 Serial = 0;

    bool operator==(const FProcedureActivationToken& Other) const
    {
        return NodeId == Other.NodeId && Serial == Other.Serial;
    }
};

USTRUCT()
struct WHFRAMEWORK_API FProcedureLink
{
    GENERATED_BODY()

    UPROPERTY()
    FGuid From;

    UPROPERTY()
    FName Output = TEXT("Out");

    UPROPERTY()
    FGuid To;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProcedureStateChanged, EProcedureState, InProcedureState);
