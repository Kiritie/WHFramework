#pragma once

#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_SetLevelOwnerPlayer.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventSetLevelOwnerPlayer : public FEventBase
{
	GENERATED_BODY()

public:
	FEventSetLevelOwnerPlayer() = default;

	FEventSetLevelOwnerPlayer(TSoftObjectPtr<UWorld> InLevelObjectPtr, FName InLevelPath, int32 InPlayerIndex)
		: LevelObjectPtr(MoveTemp(InLevelObjectPtr)), LevelPath(MoveTemp(InLevelPath)), PlayerIndex(MoveTemp(InPlayerIndex))
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UWorld> LevelObjectPtr = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditConditionHides, EditCondition = "!LevelObjectPtr.IsValid()"))
	FName LevelPath = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PlayerIndex = -1;
};
