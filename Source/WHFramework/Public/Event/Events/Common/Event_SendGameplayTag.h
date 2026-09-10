#pragma once

#include "Common/CommonModuleTypes.h"
#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_SendGameplayTag.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventSendGameplayTag : public FEventBase
{
	GENERATED_BODY()

public:
	FEventSendGameplayTag() = default;

	FEventSendGameplayTag(FGameplayTag InTag)
		: Tag(MoveTemp(InTag))
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag Tag = FGameplayTag();
};
