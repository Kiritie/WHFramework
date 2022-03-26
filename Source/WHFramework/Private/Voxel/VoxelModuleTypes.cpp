// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/VoxelModuleTypes.h"

#include "Voxel/Voxels/Voxel.h"
#include "Voxel/Voxels/VoxelPlant.h"

const FIndex FIndex::ZeroIndex = FIndex(0, 0, 0);

const FIndex FIndex::OneIndex = FIndex(1, 1, 1);

FWorldSaveData FWorldSaveData::Empty = FWorldSaveData();

FVoxelItem FVoxelItem::EmptyVoxel = FVoxelItem(EVoxelType::Empty);

FVoxelItem FVoxelItem::UnknownVoxel = FVoxelItem(EVoxelType::Unknown);

bool FVoxelItem::IsValid() const
{
	return Super::IsValid() && !IsEmpty() && !IsUnknown();
}

bool FVoxelItem::IsEmpty() const
{
	return *this == EmptyVoxel;
}

bool FVoxelItem::IsUnknown() const
{
	return *this == UnknownVoxel;
}

UVoxel* FVoxelItem::GetVoxel() const
{
	if(IsEmpty()) return UVoxel::EmptyVoxel;
	if(IsUnknown()) return UVoxel::UnknownVoxel;
	return UVoxel::LoadVoxel(Owner, *this);
}

UVoxelAssetBase& FVoxelItem::GetVoxelData() const
{
	return UPrimaryAssetManager::LoadItemAsset<UVoxelAssetBase>(ID);
}

FString FVoxelItem::GetStringData() const
{
	FString tmpData;
	if(UVoxel* tmpVoxel = GetVoxel())
	{
		tmpData = tmpVoxel->ToData();
		UVoxel::DespawnVoxel(tmpVoxel);
	}
	return tmpData;
}

bool FVoxelItem::HasParam(FName InName) const
{
	return Params.Contains(InName);
}

FParameter FVoxelItem::GetParam(FName InName)
{
	if(HasParam(InName))
	{
		return Params[InName];
	}
	return FParameter();
}

void FVoxelItem::SetParam(FName InName, FParameter InParam)
{
	if(HasParam(InName))
	{
		Params[InName] = InParam;
	}
	else
	{
		Params.Add(InName, InParam);
	}
}

FVoxelHitResult::FVoxelHitResult()
{
	FMemory::Memzero(this, sizeof(FVoxelHitResult));
	VoxelItem = FVoxelItem();
	Point = FVector();
	Normal = FVector();
}

FVoxelHitResult::FVoxelHitResult(const FVoxelItem& InVoxelItem, FVector InPoint, FVector InNormal)
{
	FMemory::Memzero(this, sizeof(FVoxelHitResult));
	VoxelItem = InVoxelItem;
	Point = InPoint;
	Normal = InNormal;
}

UVoxel* FVoxelHitResult::GetVoxel() const
{
	return VoxelItem.GetVoxel();
}

AVoxelChunk* FVoxelHitResult::GetOwner() const
{
	return VoxelItem.Owner;
}
