// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Auxiliary/VoxelAuxiliary.h"

#include "Math/MathHelper.h"
#include "Voxel/VoxelModule.h"

AVoxelAuxiliary::AVoxelAuxiliary()
{
	PrimaryActorTick.bCanEverTick = false;

	VoxelItem = FVoxelItem::Empty;
	VoxelScope = EVoxelScope::None;
}

void AVoxelAuxiliary::OnSpawn_Implementation(const FParameter& InParam)
{
	Super::OnSpawn_Implementation(InParam);
}

void AVoxelAuxiliary::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	Super::OnDespawn_Implementation(InMode);

	VoxelItem = FVoxelItem::Empty;
	VoxelScope = EVoxelScope::None;
}

void AVoxelAuxiliary::LoadData(const FParameter& InSaveData, EPhase InPhase)
{
	const auto& SaveData = InSaveData.GetRef<FVoxelAuxiliarySaveData>();

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

FParameter AVoxelAuxiliary::ToData()
{
	FVoxelAuxiliarySaveData SaveData;

	SaveData.VoxelItem = VoxelItem;
	SaveData.VoxelScope = VoxelScope;

	return FParameter(MoveTemp(SaveData));
}

FVoxelItem& AVoxelAuxiliary::GetVoxelItem(bool bRefresh)
{
	if(bRefresh) VoxelItem.RefreshData(true);
	return VoxelItem;
}
