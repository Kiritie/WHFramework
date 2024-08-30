// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "EffectBase.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UEffectBase : public UGameplayEffect, public IObjectPoolInterface
{
	GENERATED_BODY()

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation(bool bRecovery) override;
};
