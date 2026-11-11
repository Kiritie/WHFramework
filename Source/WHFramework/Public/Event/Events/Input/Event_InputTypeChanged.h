#pragma once

#include "CommonInputBaseTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_InputTypeChanged.generated.h"

class APlayerController;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventInputTypeChanged : public FEventBase
{
	GENERATED_BODY()

public:
	FEventInputTypeChanged() = default;

	FEventInputTypeChanged(
		APlayerController* InPlayer,
		ECommonInputType InInputType)
		: Player(InPlayer),
		InputType(InInputType)
	{
	}

	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<APlayerController> Player = nullptr;

	UPROPERTY(BlueprintReadWrite)
	ECommonInputType InputType = ECommonInputType::MouseAndKeyboard;
};
