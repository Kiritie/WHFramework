﻿// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Spawner/AbilityVitalitySpawner.h"

#include "Ability/AbilityModuleStatics.h"
#include "Ability/AbilityModuleTypes.h"
#include "Ability/Vitality/AbilityVitalityBase.h"
#include "Ability/Vitality/AbilityVitalityDataBase.h"
#include "Components/CapsuleComponent.h"

AAbilityVitalitySpawner::AAbilityVitalitySpawner()
{
	GetCapsuleComponent()->InitCapsuleSize(50.0f, 50.0f);
}

AActor* AAbilityVitalitySpawner::SpawnImpl_Implementation(const FAbilityItem& InAbilityItem)
{
	const auto& VitalityData = InAbilityItem.GetData<UAbilityVitalityDataBase>();
	
	auto SaveData = FVitalitySaveData();
	SaveData.AssetID = VitalityData.GetPrimaryAssetId();
	SaveData.Name = *VitalityData.Name.ToString();
	SaveData.RaceID = NAME_None;
	SaveData.Level = InAbilityItem.Level;
	SaveData.SpawnLocation = GetActorLocation();
	SaveData.SpawnRotation = GetActorRotation();
	SaveData.InventoryData = VitalityData.InventoryData;

	AAbilityVitalityBase* Vitality = UAbilityModuleStatics::SpawnAbilityVitality(&SaveData);

	return Vitality;
}

void AAbilityVitalitySpawner::DestroyImpl_Implementation(AActor* InAbilityActor)
{
	Super::DestroyImpl_Implementation(InAbilityActor);
}
