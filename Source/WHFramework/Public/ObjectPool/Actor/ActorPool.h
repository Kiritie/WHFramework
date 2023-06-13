// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ObjectPool/ObjectPool.h"

#include "UObject/NoExportTypes.h"
#include "ActorPool.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UActorPool : public UObjectPool
{
	GENERATED_BODY()

public:
	UActorPool();

public:
	virtual UObject* SpawnImpl(UObject* InObject) override;

	virtual void DespawnImpl(UObject* InObject, bool bRecovery) override;
};
