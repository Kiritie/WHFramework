#pragma once

#include "Input/InputModuleTypes.h"
#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_InputModeChanged.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventInputModeChanged : public FEventBase
{
	GENERATED_BODY()

public:
	FEventInputModeChanged() = default;

	FEventInputModeChanged(EInputMode InInputMode)
		: InputMode(MoveTemp(InInputMode))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	EInputMode InputMode = EInputMode::None;
};
