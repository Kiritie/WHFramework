#pragma once

#include "WHFrameworkSlateTypes.h"
#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_UserWidgetStateChanged.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventUserWidgetStateChanged : public FEventBase
{
	GENERATED_BODY()

public:
	FEventUserWidgetStateChanged() = default;

	FEventUserWidgetStateChanged(class UUserWidgetBase* InUserWidget, EScreenWidgetState InWidgetState)
		: UserWidget(MoveTemp(InUserWidget)), WidgetState(MoveTemp(InWidgetState))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	class UUserWidgetBase* UserWidget = nullptr;

	UPROPERTY(BlueprintReadWrite)
	EScreenWidgetState WidgetState = EScreenWidgetState::None;
};
