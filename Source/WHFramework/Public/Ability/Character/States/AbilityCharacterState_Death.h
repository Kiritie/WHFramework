// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilityCharacterStateBase.h"
#include "UObject/Object.h"
#include "AbilityCharacterState_Death.generated.h"

class UAnimMontage;
class IAbilityVitalityInterface;
class AAbilityCharacterBase;
/**
 *
 */
UCLASS()
class WHFRAMEWORK_API UAbilityCharacterState_Death : public UAbilityCharacterStateBase
{
	GENERATED_BODY()

	friend class AAbilityCharacterBase;

public:
	UAbilityCharacterState_Death();

public:
	virtual void OnInitialize(UFSMComponent* InFSM, int32 InStateIndex) override;

	virtual bool OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams) override;

	virtual void OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams) override;

	virtual void OnRefresh(float DeltaSeconds) override;

	virtual bool OnPreLeave(UFiniteStateBase* InNextState) override;

	virtual void OnLeave(UFiniteStateBase* InNextState) override;

	virtual void OnTermination() override;

public:
	UFUNCTION(BlueprintCallable)
	virtual void DeathStart();
	bool IsDeathPresentationFinished() const;
	virtual void OnPresentationEnter(UFiniteStateBase* Last, const FFSMReplicatedState& State) override;
	virtual void OnPresentationLeave(UFiniteStateBase* Next) override;
	UPROPERTY(EditDefaultsOnly)
	float DeathPresentationDuration = 2.f;
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UAnimMontage> DeathMontage = nullptr;

	UFUNCTION(BlueprintCallable)
	virtual void DeathEnd();

protected:
	TWeakObjectPtr<UObject> Killer;
	FGuid MovementSource;
	bool bDeathEnded = false;
	bool bRestoringDeath = false;

	bool bDeathStarted;
};
