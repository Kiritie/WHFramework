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
		SetActorRelativeLocation(VoxelItem.GetLocation() + VoxelItem.GetRange() * AVoxelModule::Get()->GetWorldData().BlockSize * 0.5f);
	}
}

FSaveData* AVoxelAuxiliary::ToData(bool bRefresh)
{
	static FVoxelAuxiliarySaveData SaveData;
	SaveData = FVoxelAuxiliarySaveData();

	SaveData.VoxelItem = VoxelItem;

	return &SaveData;
}

FVoxelItem& AVoxelAuxiliary::GetVoxelItem(bool bRefresh)
{
	if(bRefresh) VoxelItem.RefreshData(true);
	return VoxelItem;
}
