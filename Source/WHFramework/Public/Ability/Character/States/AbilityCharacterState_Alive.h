#pragma once
#include "Ability/Character/States/AbilityCharacterStateBase.h"
#include "TimerManager.h"
#include "AbilityCharacterState_Alive.generated.h"

UCLASS()
class WHFRAMEWORK_API UAbilityCharacterState_Alive : public UAbilityCharacterStateBase
{
	GENERATED_BODY()
public:
	UAbilityCharacterState_Alive();
	virtual bool OnPreEnter(UFiniteStateBase* Last, const TArray<FParameter>& Params) override;
	virtual void OnPresentationEnter(UFiniteStateBase* Last, const FFSMReplicatedState& State) override;
	virtual void OnPresentationLeave(UFiniteStateBase* Next) override;
	virtual void OnEnter(UFiniteStateBase* Last, const TArray<FParameter>& Params) override;
	virtual void OnLeave(UFiniteStateBase* Next) override;
	virtual void OnTermination() override;

private:
	void TryResumeMovement();
	void ClearMovementRetry();
	FTimerHandle MovementRetryTimer;
};
