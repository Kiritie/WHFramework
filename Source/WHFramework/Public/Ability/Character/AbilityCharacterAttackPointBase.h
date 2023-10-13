// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/BoxComponent.h"
#include "Ability/Interfaces/AbilityAttackerInterface.h"

#include "AbilityCharacterAttackPointBase.generated.h"

class AAbilityCharacterBase;

/**
 * 角色攻击点组件
 */
UCLASS()
class WHFRAMEWORK_API UAbilityCharacterAttackPointBase : public UBoxComponent, public IAbilityAttackerInterface
{
	GENERATED_BODY()
	
public:
	UAbilityCharacterAttackPointBase(const FObjectInitializer& ObjectInitializer);

protected:
	UPROPERTY()
	TArray<AActor*> HitTargets;

protected:
	UFUNCTION()
	void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult);

public:
	virtual bool CanHitTarget(AActor* InTarget) const override;

	virtual void OnHitTarget(AActor* InTarget, const FHitResult& InHitResult) override;
	
	virtual void ClearHitTargets();

	virtual void SetHitAble(bool bValue);

public:
	UFUNCTION(BlueprintPure)
	AAbilityCharacterBase* GetOwnerCharacter() const;
};
