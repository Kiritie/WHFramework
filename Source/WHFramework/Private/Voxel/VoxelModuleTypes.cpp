// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/VoxelModuleTypes.h"

#include "Voxel/VoxelModuleBPLibrary.h"
#include "Voxel/Voxels/Voxel.h"
#include "Voxel/Voxels/VoxelPlant.h"

FVoxelWorldSaveData FVoxelWorldSaveData::Empty = FVoxelWorldSaveData();

FVoxelItem FVoxelItem::Empty = FVoxelItem();
FVoxelItem FVoxelItem::Unknown = FVoxelItem(FPrimaryAssetId(FName("Voxel"), FName("DA_Voxel_Unknown")));

FVoxelItem::FVoxelItem(EVoxelType InVoxelType, bool bInitSaveData) : FVoxelItem()
{
	VoxelType = InVoxelType;
	ID = UVoxelModuleBPLibrary::VoxelTypeToAssetID(InVoxelType);
	if(bInitSaveData)
	{
		RefreshSaveData();
	}
}

FVoxelItem::FVoxelItem(const FPrimaryAssetId& InID, bool bInitSaveData) : FVoxelItem()
{
	ID = InID;
	if(bInitSaveData)
	{
		RefreshSaveData();
	}
}

FVoxelItem::FVoxelItem(const FVoxelSaveData& InSaveData) : FVoxelSaveData(InSaveData)
{
	ID = UVoxelModuleBPLibrary::VoxelTypeToAssetID(InSaveData.VoxelType);
	RefreshSaveData();
}

bool FVoxelItem::IsValid(bool bNeedNotNull) const
{
	return Super::IsValid(bNeedNotNull) && !IsUnknown();
}

bool FVoxelItem::IsUnknown() const
{
	return *this == Unknown;
}

void FVoxelItem::RefreshSaveData()
{
	*this = GetVoxel().ToSaveDataRef<FVoxelItem>(true);
}

UVoxel& FVoxelItem::GetVoxel() const
{
	return UVoxelModuleBPLibrary::GetVoxel(*this);
}

AVoxelChunk* FVoxelItem::GetOwner() const
{
	if(Owner) return Owner;
	return UVoxelModuleBPLibrary::FindChunkByIndex(Index);
}

FVoxelHitResult::FVoxelHitResult()
{
	// FMemory::Memzero(this, sizeof(FVoxelHitResult));
	VoxelItem = FVoxelItem();
	Point = FVector();
	Normal = FVector();
}

FVoxelHitResult::FVoxelHitResult(const FVoxelItem& InVoxelItem, FVector InPoint, FVector InNormal)
{
	// FMemory::Memzero(this, sizeof(FVoxelHitResult));
	VoxelItem = InVoxelItem;
	Point = InPoint;
	Normal = InNormal;
}

UVoxel& FVoxelHitResult::GetVoxel() const
{
	return VoxelItem.GetVoxel();
}

AVoxelChunk* FVoxelHitResult::GetChunk() const
{
	return VoxelItem.Owner;
}
