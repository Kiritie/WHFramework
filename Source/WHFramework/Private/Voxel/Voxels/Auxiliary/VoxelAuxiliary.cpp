// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Auxiliary/VoxelAuxiliary.h"

#include "Voxel/VoxelModule.h"

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
		switch(SaveData.MeshNature)
		{
			case EVoxelMeshNature::Chunk:
			{
				SetActorRelativeLocation(VoxelItem.GetLocation() + VoxelItem.GetRange() * UVoxelModule::Get()->GetWorldData().BlockSize * 0.5f);
				break;
			}
			case EVoxelMeshNature::Vitality:
			{
				SetActorRelativeLocation(FVector(0.f, 0.f, VoxelItem.GetRange().Z * UVoxelModule::Get()->GetWorldData().BlockSize * 0.5f));
				break;
			}
			default: break;
		}
	}
}

FSaveData* AVoxelAuxiliary::ToData()
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
