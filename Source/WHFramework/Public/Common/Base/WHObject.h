// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ObjectPool/ObjectPoolInterface.h"

#include "WHObject.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, hidecategories = (Tick, Replication, Rendering, Collision, Actor, Input, LOD, Cooking, Hidden, WorldPartition, Hlod, DataLayers))
class WHFRAMEWORK_API UWHObject : public UObject, public IObjectPoolInterface
{
	GENERATED_BODY()

public:
	UWHObject();

	UWHObject(const FObjectInitializer& ObjectInitializer);

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:

	virtual void OnSpawn_Implementation(const FParameter& InParam) override;
		
	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode)
		override;

public:
	virtual UWorld* GetWorld() const override;
};
