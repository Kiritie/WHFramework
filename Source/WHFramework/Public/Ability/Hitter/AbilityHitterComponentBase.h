// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilityHitterInterface.h"
#include "Components/BoxComponent.h"

#include "AbilityHitterComponentBase.generated.h"

/**
 * 攻击点组件
 */
UCLASS()
class WHFRAMEWORK_API UAbilityHitterComponentBase : public UBoxComponent, public IAbilityHitterInterface
{
	GENERATED_BODY()
	
public:
	UAbilityHitterComponentBase(const FObjectInitializer& ObjectInitializer);

protected:
	UFUNCTION()
	void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult);

public:
	virtual bool CanHitTarget(AActor* InTarget) const override;

	virtual void OnHitTarget(AActor* InTarget, const FHitResult& InHitResult) override;

public:
	virtual bool IsHitAble() const override;
	
	virtual void SetHitAble(bool bValue) override;
	
	virtual void ClearHitTargets() override;

	virtual TArray<AActor*> GetHitTargets() const override;

protected:
	UPROPERTY()
	TArray<AActor*> HitTargets;

public:
	template<class T>
	T* GetOwnerActor() const
	{
		return Cast<T>(GetOwnerActor());
	}

	AActor* GetOwnerActor() const;
};
