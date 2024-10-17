// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Spawner/AbilityVitalitySpawner.h"

#include "Ability/AbilityModuleStatics.h"
#include "Ability/AbilityModuleTypes.h"
#include "Ability/Vitality/AbilityVitalityBase.h"
#include "Ability/Vitality/AbilityVitalityDataBase.h"
#include "Components/CapsuleComponent.h"

AAbilityVitalitySpawner::AAbilityVitalitySpawner()
{
}

AActor* AAbilityVitalitySpawner::SpawnImpl_Implementation(const FAbilityItem& InAbilityItem)
{
	const auto& VitalityData = InAbilityItem.GetData<UAbilityVitalityDataBase>();
	
	auto SaveData = FVitalitySaveData();
	SaveData.AssetID = VitalityData.GetPrimaryAssetId();
	SaveData.Name = *VitalityData.Name.ToString();
	SaveData.RaceID = VitalityData.RaceID;
	SaveData.Level = InAbilityItem.Level;
	SaveData.SpawnTransform = GetActorTransform();
	SaveData.InventoryData = VitalityData.InventoryData;

	return UAbilityModuleStatics::SpawnAbilityActor(&SaveData);
}

void AAbilityVitalitySpawner::DestroyImpl_Implementation(AActor* InAbilityActor)
{
	Super::DestroyImpl_Implementation(InAbilityActor);
}
