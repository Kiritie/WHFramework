#pragma once

#include "Common/CommonModuleTypes.h"
#include "InteractionTypes.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInteractionContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	AActor* Interactor = nullptr;

	UPROPERTY(BlueprintReadOnly)
	AActor* Target = nullptr;

	UPROPERTY(BlueprintReadOnly)
	APlayerController* Player = nullptr;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInteractionOptionView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FName OptionID;

	UPROPERTY(BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly)
	FText DisabledReason;

	UPROPERTY(BlueprintReadOnly)
	bool bEnabled = false;

	UPROPERTY(BlueprintReadOnly)
	int32 Priority = 0;

	UPROPERTY(BlueprintReadOnly)
	EInteractAction LegacyAction = EInteractAction::None;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInteractionOptionsChanged);
