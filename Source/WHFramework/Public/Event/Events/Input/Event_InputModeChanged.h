#pragma once

#include "Input/InputModuleTypes.h"
#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_InputModeChanged.generated.h"

class APlayerController;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventInputModeChanged : public FEventBase
{
	GENERATED_BODY()

public:
	FEventInputModeChanged() = default;

	FEventInputModeChanged(
		APlayerController* InPlayer,
		EInputMode InInputMode,
		EInputMode InPreviousInputMode = EInputMode::None)
		: Player(InPlayer),
		InputMode(InInputMode),
		PreviousInputMode(InPreviousInputMode)
	{
	}

	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<APlayerController> Player = nullptr;

	UPROPERTY(BlueprintReadWrite)
	EInputMode InputMode = EInputMode::None;

	UPROPERTY(BlueprintReadWrite)
	EInputMode PreviousInputMode = EInputMode::None;
};
