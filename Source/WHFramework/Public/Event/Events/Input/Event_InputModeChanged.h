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

	FEventInputModeChanged(
		EInputMode InInputMode,
		EInputMode InPreviousInputMode = EInputMode::None)
		: InputMode(InInputMode),
		PreviousInputMode(InPreviousInputMode)
	{
	}

	UPROPERTY(BlueprintReadWrite)
	EInputMode InputMode = EInputMode::None;

	UPROPERTY(BlueprintReadWrite)
	EInputMode PreviousInputMode = EInputMode::None;
};
