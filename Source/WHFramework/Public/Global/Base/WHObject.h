// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "ReferencePool/ReferencePoolInterface.h"

#include "WHObject.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, meta = (ShowWorldContextPin), hidecategories = (Tick, Replication, Rendering, Collision, Actor, Input, LOD, Cooking, Hidden, WorldPartition, Hlod, DataLayers))
class WHFRAMEWORK_API UWHObject : public UObject, public IObjectPoolInterface, public IReferencePoolInterface
{
	GENERATED_BODY()

public:
	UWHObject();

	//////////////////////////////////////////////////////////////////////////
	/// Object
public:
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation() override;

	virtual void OnReset_Implementation() override;
};
