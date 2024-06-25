// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Main/Base/ModuleNetworkComponentBase.h"

#include "AbilityModuleNetworkComponent.generated.h"

class AAbilitySpawnerBase;
/**
* 
*/
UCLASS()
class WHFRAMEWORK_API UAbilityModuleNetworkComponent : public UModuleNetworkComponentBase
{
	GENERATED_BODY()

public:
	UAbilityModuleNetworkComponent();
	
public:
	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerSpawnerSpawn(AAbilitySpawnerBase* InAbilitySpawner);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerSpawnerDestroy(AAbilitySpawnerBase* InAbilitySpawner);
};
