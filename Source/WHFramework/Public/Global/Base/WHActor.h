// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "GameFramework/Actor.h"
#include "WHActor.generated.h"

/**
 * 
 */
UCLASS(hidecategories = (Tick, Replication, Rendering, Collision, Actor, Input, LOD, Cooking, Hidden, WorldPartition, Hlod))
class WHFRAMEWORK_API AWHActor : public AActor
{
	GENERATED_BODY()
	
public:	
	AWHActor();

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void SetVisible(bool bVisible);
};
