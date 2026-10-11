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

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Persistence")
	bool bPersistAcrossSave = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Persistence")
	bool bAdvanceWhileOffline = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Persistence")
	bool bRemoveOnDeath = true;

	bool SupportsPersistence(float Level = 1.f) const;
	bool SupportsAttributeGrant(float Level = 1.f) const;


	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:

	virtual void OnSpawn_Implementation(const FParameter& InParam) override;

	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode)
		override;
};
