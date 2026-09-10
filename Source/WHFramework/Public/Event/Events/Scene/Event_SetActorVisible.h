#pragma once

#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_SetActorVisible.generated.h"

class UDataLayerAsset;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventSetActorVisible : public FEventBase
{
	GENERATED_BODY()

public:
	FEventSetActorVisible() = default;

	FEventSetActorVisible(TSoftObjectPtr<AActor> InActorPath, bool InbVisible)
		: ActorPath(MoveTemp(InActorPath)), bVisible(MoveTemp(InbVisible))
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<AActor> ActorPath = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bVisible = false;
};
