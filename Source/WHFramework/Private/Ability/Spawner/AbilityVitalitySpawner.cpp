// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Spawner/AbilityVitalitySpawner.h"
#include "Ability/AbilityActorSaveTypes.h"
#include "Ability/Inventory/InventoryItemTypes.h"

#include "Ability/AbilityModuleStatics.h"
#include "Ability/Inventory/InventoryItemAssetCatalog.h"
#include "Ability/Inventory/InventoryModule.h"
#include "Engine/World.h"
#include "Ability/Vitality/AbilityVitalityBase.h"
#include "Ability/Vitality/AbilityVitalityDataBase.h"
#include "Components/CapsuleComponent.h"

AAbilityVitalitySpawner::AAbilityVitalitySpawner()
{
}

AActor* AAbilityVitalitySpawner::SpawnImpl_Implementation(const FAbilityItem& InAbilityItem)
{
	const UInventoryModule* Subsystem = GetWorld() ? UInventoryModule::GetPtr(GetWorld()) : nullptr;
	const UInventoryItemAssetCatalog* Catalog = Subsystem ? Subsystem->GetItemAssets() : nullptr;
	const UAbilityVitalityDataBase* Definition = Catalog ? Catalog->FindAs<UAbilityVitalityDataBase>(InAbilityItem.ItemId) : nullptr;
	if (!Definition || InAbilityItem.Level < 0 || InAbilityItem.Level > Definition->MaxLevel)
	{
		return nullptr;
	}
	const UAbilityVitalityDataBase& VitalityData = *Definition;
	
	auto SaveData = FVitalitySaveData();
	SaveData.AssetID = VitalityData.GetPrimaryAssetId();
	SaveData.Name = *VitalityData.Name.ToString();
	SaveData.RaceID = VitalityData.RaceID;
	SaveData.Level = InAbilityItem.Level;
	SaveData.SpawnTransform = GetActorTransform();

	return UAbilityModuleStatics::SpawnAbilityActor(GetWorld(), FParameter(MoveTemp(SaveData)));
}

void AAbilityVitalitySpawner::DestroyImpl_Implementation(AActor* InAbilityActor)
{
	Super::DestroyImpl_Implementation(InAbilityActor);
}

