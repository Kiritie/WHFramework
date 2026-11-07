#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "InteractionTypes.generated.h"

class UInteractionOptionBase;
class AActor;
class APlayerController;

UENUM(BlueprintType)
enum class EInteractAgentType : uint8
{
	// 无
	None = 0,
	// 被动的
	Passivity,
	// 主动的
	Initiative
};

class UInteractionActionExecution;

UENUM(BlueprintType)
enum class EInteractionActionState : uint8
{
	None,
	Running,
	Completed,
	Failed,
	Cancelled
};

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

	UPROPERTY(BlueprintReadOnly)
	UInteractionActionExecution* Execution = nullptr;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInteractionOptionView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	UInteractionOptionBase* Option = nullptr;

	UPROPERTY(BlueprintReadOnly)
	FGameplayTag OptionTag;

	UPROPERTY(BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly)
	FText DisabledReason;

	UPROPERTY(BlueprintReadOnly)
	bool bEnabled = false;

	UPROPERTY(BlueprintReadOnly)
	int32 Priority = 0;

};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInteractionOptionsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInteractionTargetsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInteractionCancelled);
