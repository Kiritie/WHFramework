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
}

void AVoxelAuxiliary::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	const auto& SaveData = InSaveData->CastRef<FVoxelAuxiliarySaveData>();

	if(PHASEC(InPhase, EPhase::All))
	{
		VoxelItem = SaveData.VoxelItem;
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

	return &SaveData;
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
