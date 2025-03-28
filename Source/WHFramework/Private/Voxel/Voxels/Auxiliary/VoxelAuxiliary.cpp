// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Auxiliary/VoxelAuxiliary.h"

#include "Math/MathHelper.h"
#include "Voxel/VoxelModule.h"

AVoxelAuxiliary::AVoxelAuxiliary()
{
	VoxelItem = FVoxelItem::Empty;
	VoxelScope = EVoxelScope::None;
}

void AVoxelAuxiliary::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);
}

void AVoxelAuxiliary::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);

	VoxelItem = FVoxelItem::Empty;
	VoxelScope = EVoxelScope::None;
}

void AVoxelAuxiliary::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	const auto& SaveData = InSaveData->CastRef<FVoxelAuxiliarySaveData>();

	if(PHASEC(InPhase, EPhase::All))
	{
		VoxelItem = SaveData.VoxelItem;
		VoxelScope = SaveData.VoxelScope;
		switch(VoxelScope)
		{
			case EVoxelScope::Chunk:
			case EVoxelScope::Prefab:
			{
				SetActorRelativeLocation(VoxelItem.GetLocation() + VoxelItem.GetRange(true, true) * UVoxelModule::Get().GetWorldData().BlockSize * 0.5f);
				break;
			}
			case EVoxelScope::Preview:
			{
				SetActorRelativeLocation(VoxelItem.GetRange(true, true) * UVoxelModule::Get().GetWorldData().BlockSize * 0.5f);
				break;
			}
			case EVoxelScope::Vitality:
			{
				SetActorRelativeLocation(FVector(0.f, 0.f, VoxelItem.GetRange().Z * UVoxelModule::Get().GetWorldData().BlockSize * 0.5f));
				break;
			}
			default: break;
		}
		SetActorRelativeRotation(FRotator(0.f, FMathHelper::RightAngleToFloat(VoxelItem.Angle), 0.f));
		SetActorRelativeScale3D(FVector::OneVector);
	}
}

FSaveData* AVoxelAuxiliary::ToData()
{
	static FVoxelAuxiliarySaveData SaveData;
	SaveData = FVoxelAuxiliarySaveData();

	SaveData.VoxelItem = VoxelItem;
	SaveData.VoxelScope = VoxelScope;

	return &SaveData;
}

FVoxelItem& AVoxelAuxiliary::GetVoxelItem(bool bRefresh)
{
	if(bRefresh) VoxelItem.RefreshData(true);
	return VoxelItem;
}
