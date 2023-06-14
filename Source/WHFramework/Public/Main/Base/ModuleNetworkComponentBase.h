// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"
#include "ModuleNetworkComponentBase.generated.h"

/**
 * 
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class WHFRAMEWORK_API UModuleNetworkComponentBase : public UActorComponent
{
	GENERATED_BODY()

public:
	UModuleNetworkComponentBase();
};
