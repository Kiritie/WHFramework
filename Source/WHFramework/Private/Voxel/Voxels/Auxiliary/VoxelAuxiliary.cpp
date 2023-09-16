// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Auxiliary/VoxelAuxiliary.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Ability/Components/VoxelInteractionComponent.h"
#include "Ability/Inventory/Inventory.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleBPLibrary.h"
#include "Voxel/Chunks/VoxelChunk.h"

AVoxelAuxiliary::AVoxelAuxiliary()
{
	Interaction = CreateDefaultSubobject<UVoxelInteractionComponent>(FName("Interaction"));
	Interaction->SetupAttachment(RootComponent);
	Interaction->SetRelativeLocation(FVector(0, 0, 0));
	
	Inventory = CreateDefaultSubobject<UInventory>(FName("Inventory"));

	VoxelItem = FVoxelItem::Empty;
}

void AVoxelAuxiliary::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InParams);
}

void AVoxelAuxiliary::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);

	VoxelItem = FVoxelItem::Empty;
	Inventory->DiscardAllItem();
	Inventory->UnloadSaveData();
}

void AVoxelAuxiliary::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FVoxelAuxiliarySaveData>();

	if(PHASEC(InPhase, EPhase::All))
	{
		VoxelItem = SaveData.VoxelItem;
		Inventory->LoadSaveData(&SaveData.InventoryData, InPhase);
		if(VoxelItem.Owner)
		{
			SetActorRelativeLocation(VoxelItem.GetLocation() + VoxelItem.GetRange() * AVoxelModule::Get()->GetWorldData().BlockSize * 0.5f);
			Interaction->SetBoxExtent(VoxelItem.GetRange() * AVoxelModule::Get()->GetWorldData().BlockSize * FVector(1.5f, 1.5f, 0.5f));
		}
	}
}

FSaveData* AVoxelAuxiliary::ToData(bool bRefresh)
{
	static FVoxelAuxiliarySaveData SaveData;
	SaveData = FVoxelAuxiliarySaveData();

	SaveData.VoxelItem = VoxelItem;

	SaveData.InventoryData = Inventory->GetSaveDataRef<FInventorySaveData>(true);

	return &SaveData;
}

void AVoxelAuxiliary::OnActiveItem(const FAbilityItem& InItem, bool bPassive, bool bSuccess)
{

}

void AVoxelAuxiliary::OnCancelItem(const FAbilityItem& InItem, bool bPassive)
{

}

void AVoxelAuxiliary::OnAssembleItem(const FAbilityItem& InItem)
{

}

void AVoxelAuxiliary::OnDischargeItem(const FAbilityItem& InItem)
{

}

void AVoxelAuxiliary::OnDiscardItem(const FAbilityItem& InItem, bool bInPlace)
{
	FVector tmpPos = GetActorLocation() + FMath::RandPointInBox(FBox(FVector(-20.f, -20.f, -10.f), FVector(20.f, 20.f, 10.f)));
	if(!bInPlace) tmpPos += GetActorForwardVector() * (Interaction->GetUnscaledBoxExtent());
	UAbilityModuleBPLibrary::SpawnPickUp(InItem, tmpPos, Container.GetInterface());
}

void AVoxelAuxiliary::OnSelectItem(const FAbilityItem& InItem)
{
	
}

void AVoxelAuxiliary::OnAuxiliaryItem(const FAbilityItem& InItem)
{

}

void AVoxelAuxiliary::OnEnterInteract(IInteractionAgentInterface* InInteractionAgent)
{
}

void AVoxelAuxiliary::OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent)
{
}

bool AVoxelAuxiliary::CanInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction)
{
	return true;
}

void AVoxelAuxiliary::OnInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction)
{
	
}

UInteractionComponent* AVoxelAuxiliary::GetInteractionComponent() const
{
	return Interaction;
}
