#pragma once

#include "Parameter/ParameterModuleTypes.h"
#include "Scene/SceneModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_AsyncUnloadLevels.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventAsyncUnloadLevels : public FEventBase
{
	GENERATED_BODY()

public:
	FEventAsyncUnloadLevels() = default;

	FEventAsyncUnloadLevels(TArray<FSoftLevelPath> InSoftLevelPaths, float InFinishDelayTime, bool InbCreateLoadingWidget)
		: SoftLevelPaths(MoveTemp(InSoftLevelPaths)), FinishDelayTime(MoveTemp(InFinishDelayTime)), bCreateLoadingWidget(MoveTemp(InbCreateLoadingWidget))
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FSoftLevelPath> SoftLevelPaths = TArray<FSoftLevelPath>();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FinishDelayTime = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCreateLoadingWidget = false;
};
