// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ObjectPool/ObjectPoolInterface.h"

#include "WHObject.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, meta = (ShowWorldContextPin), hidecategories = (Tick, Replication, Rendering, Collision, Actor, Input, LOD, Cooking, Hidden, WorldPartition, Hlod, DataLayers))
class WHFRAMEWORK_API UWHObject : public UObject, public IObjectPoolInterface
{
	GENERATED_BODY()

public:
	UWHObject();

	//////////////////////////////////////////////////////////////////////////
	/// Object
public:
	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation() override;
};
