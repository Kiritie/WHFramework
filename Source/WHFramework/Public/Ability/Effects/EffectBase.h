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

	virtual void OnSpawn_Implementation(
		const FParameter& InParameter) override;
		
	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode)
		override;
};
