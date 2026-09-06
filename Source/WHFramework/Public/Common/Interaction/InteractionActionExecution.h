#pragma once

#include "Common/CommonModuleTypes.h"
#include "UObject/Object.h"
#include "InteractionActionExecution.generated.h"

class UInteractionActionBase;

/** 单次交互动作的运行状态 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UInteractionActionExecution : public UObject
{
	GENERATED_BODY()
public:
	bool Start(UInteractionActionBase* InAction, const FInteractionContext& InContext, FGameplayTag InOptionTag, FText& OutReason);

	UFUNCTION(BlueprintCallable)
	void Finish(bool bSucceeded = true);

	UFUNCTION(BlueprintCallable)
	void Cancel();

	UFUNCTION(BlueprintCallable)
	void DeferCompletion() { bWaitForFinish = true; }

	UFUNCTION(BlueprintPure)
	EInteractionActionState GetState() const { return State; }

	UFUNCTION(BlueprintPure)
	FInteractionContext GetContext() const { return Context; }

	FGameplayTag GetOptionTag() const { return OptionTag; }
	virtual UWorld* GetWorld() const override;

private:
	UFUNCTION()
	void OnParticipantDestroyed(AActor* InActor);

	void End(EInteractionActionState InState);

	UPROPERTY(Transient)
	UInteractionActionBase* Action = nullptr;

	UPROPERTY(Transient)
	FInteractionContext Context;

	FGameplayTag OptionTag;
	EInteractionActionState State = EInteractionActionState::None;
	bool bWaitForFinish = false;
	bool bStarting = false;
	EInteractionActionState PendingState = EInteractionActionState::None;
};
