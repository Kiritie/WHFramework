
// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Auxiliary/VoxelContainerAuxiliary.h"

#include "Ability/AbilityModuleStatics.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Common/Interaction/InteractionComponent.h"
#include "Voxel/Datas/VoxelContainerData.h"

// Sets default values
AVoxelContainerAuxiliary::AVoxelContainerAuxiliary()
{
	Inventory = CreateDefaultSubobject<UAbilityInventoryBase>(FName("Inventory"));
}

void AVoxelContainerAuxiliary::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);

	Inventory->UnloadSaveData();
}

void AVoxelContainerAuxiliary::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	Super::LoadData(InSaveData, InPhase);
	
	auto& SaveData = InSaveData->CastRef<FVoxelAuxiliarySaveData>();

	if(!SaveData.InventoryData.IsSaved())
	{
		SaveData.InventoryData = VoxelItem.GetVoxelData<UVoxelContainerData>().InventoryData;
	}

	if(PHASEC(InPhase, EPhase::All))
	{
		Inventory->LoadSaveData(&SaveData.InventoryData, InPhase);
	}
}

FSaveData* AVoxelContainerAuxiliary::ToData()
{
	auto& SaveData = Super::ToData()->CastRef<FVoxelAuxiliarySaveData>();

	SaveData.InventoryData = Inventory->GetSaveDataRef<FInventorySaveData>(true);

	return &SaveData;
}

void AVoxelContainerAuxiliary::OnAdditionItem(const FAbilityItem& InItem)
{
	
}

void AVoxelContainerAuxiliary::OnRemoveItem(const FAbilityItem& InItem)
{
	
}

void AVoxelContainerAuxiliary::OnChangeItem(const FAbilityItem& InNewItem, FAbilityItem& InOldItem)
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
