// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "WHObject.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, meta = (ShowWorldContextPin), hidecategories = (Tick, Replication, Rendering, Collision, Actor, Input, LOD, Cooking, Hidden, WorldPartition, Hlod, DataLayers))
class WHFRAMEWORK_API UWHObject : public UObject
{
	GENERATED_BODY()

public:
	UWHObject();
};
