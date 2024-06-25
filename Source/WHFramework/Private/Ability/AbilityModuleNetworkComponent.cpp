// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/AbilityModuleNetworkComponent.h"

#include "Ability/Spawner/AbilitySpawnerBase.h"

UAbilityModuleNetworkComponent::UAbilityModuleNetworkComponent()
{
	
}

bool UAbilityModuleNetworkComponent::ServerSpawnerSpawn_Validate(AAbilitySpawnerBase* InAbilitySpawner) { return true; }
void UAbilityModuleNetworkComponent::ServerSpawnerSpawn_Implementation(AAbilitySpawnerBase* InAbilitySpawner)
{
	InAbilitySpawner->Spawn();
}

bool UAbilityModuleNetworkComponent::ServerSpawnerDestroy_Validate(AAbilitySpawnerBase* InAbilitySpawner) { return true; }
void UAbilityModuleNetworkComponent::ServerSpawnerDestroy_Implementation(AAbilitySpawnerBase* InAbilitySpawner)
{
	InAbilitySpawner->Destroy();
}
