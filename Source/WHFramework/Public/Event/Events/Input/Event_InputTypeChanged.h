#pragma once

#include "CommonInputBaseTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_InputTypeChanged.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventInputTypeChanged : public FEventBase
{
	GENERATED_BODY()

public:
	FEventInputTypeChanged() = default;

	FEventInputTypeChanged(
		int32 InPlayerIndex,
		ECommonInputType InInputType)
		: PlayerIndex(InPlayerIndex),
		InputType(InInputType)
	{
	}

	UPROPERTY(BlueprintReadWrite)
	int32 PlayerIndex = 0;

	UPROPERTY(BlueprintReadWrite)
	ECommonInputType InputType = ECommonInputType::MouseAndKeyboard;
};
