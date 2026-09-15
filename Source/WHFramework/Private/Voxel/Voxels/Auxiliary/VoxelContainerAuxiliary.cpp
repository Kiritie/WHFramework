
// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Auxiliary/VoxelContainerAuxiliary.h"

#include "Ability/AbilityModuleStatics.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Common/Interaction/InteractionComponent.h"
#include "Voxel/Voxels/Data/VoxelContainerData.h"

// Sets default values
AVoxelContainerAuxiliary::AVoxelContainerAuxiliary()
{
	Inventory = CreateDefaultSubobject<UAbilityInventoryBase>(FName("Inventory"));
}

void AVoxelContainerAuxiliary::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	Super::OnDespawn_Implementation(InMode);

	Inventory->UnloadSaveData();
}

void AVoxelContainerAuxiliary::LoadData(const FParameter& InSaveData, EPhase InPhase)
{
	Super::LoadData(InSaveData, InPhase);
	
	const FVoxelAuxiliarySaveData& SaveData = InSaveData.GetRef<FVoxelAuxiliarySaveData>();
	FParameter InventoryData = SaveData.InventoryData;

	if(!InventoryData.HasValue() || !InventoryData.GetRef<FInventorySaveData>().IsSaved())
	{
		InventoryData = FParameter(VoxelItem.GetData<UVoxelContainerData>().InventoryData);
	}

	if(PHASEC(InPhase, EPhase::All))
	{
		Inventory->LoadSaveData(InventoryData, InPhase);
	}
}

FParameter AVoxelContainerAuxiliary::ToData()
{
	FParameter Result = Super::ToData();
	FVoxelAuxiliarySaveData& SaveData = *Result.GetMutablePtr<FVoxelAuxiliarySaveData>();

	SaveData.InventoryData = Inventory->GetSaveData(true);

	return Result;
}

void AVoxelContainerAuxiliary::OnAdditionItem(const FAbilityItem& InItem)
{
	
}

void AVoxelContainerAuxiliary::OnRemoveItem(const FAbilityItem& InItem)
{
	
}

void AVoxelContainerAuxiliary::OnPreChangeItem(const FAbilityItem& InOldItem)
{
	
}

void AVoxelContainerAuxiliary::OnChangeItem(const FAbilityItem& InNewItem)
{
	
}

void AVoxelContainerAuxiliary::OnActiveItem(const FAbilityItem& InItem, bool bPassive, bool bSuccess)
{

}

void AVoxelContainerAuxiliary::OnDeactiveItem(const FAbilityItem& InItem, bool bPassive)
{

}

void AVoxelContainerAuxiliary::OnDiscardItem(const FAbilityItem& InItem, bool bInPlace)
{
	FVector Pos = GetActorLocation() + FMath::RandPointInBox(FBox(FVector(-20.f, -20.f, -10.f), FVector(20.f, 20.f, 10.f)));
	if(!bInPlace) Pos += GetActorForwardVector() * (Interaction->GetUnscaledBoxExtent());
	UAbilityModuleStatics::SpawnAbilityPickUp(InItem, Pos, Container.GetInterface());
}

void AVoxelContainerAuxiliary::OnSelectItem(const FAbilityItem& InItem)
{
	
}

void AVoxelContainerAuxiliary::OnAuxiliaryItem(const FAbilityItem& InItem)
{

}
