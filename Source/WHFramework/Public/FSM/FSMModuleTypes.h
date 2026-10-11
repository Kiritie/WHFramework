#pragma once
#include "CoreMinimal.h"
#include "FSMModuleTypes.generated.h"

class UFSMComponent;
class UFiniteStateBase;

UENUM(BlueprintType)
enum class EFSMExecutionPolicy : uint8
{
	None,
	Authority,
	Local
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FFSMReplicatedState
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly)
	FName Domain = NAME_None;
	UPROPERTY(BlueprintReadOnly)
	FName State = NAME_None;
	UPROPERTY(BlueprintReadOnly)
	int64 Revision = 0;
	UPROPERTY(BlueprintReadOnly)
	double StartedAt = 0.0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFiniteStateChanged, UFiniteStateBase*, InCurrentState, UFiniteStateBase*, InLastState);

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FFSMGroup
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<UFSMComponent*> FSMArray;
};
