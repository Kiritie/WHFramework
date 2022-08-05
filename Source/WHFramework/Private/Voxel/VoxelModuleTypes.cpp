// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/VoxelModuleTypes.h"

#include "Voxel/VoxelModuleBPLibrary.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Datas/VoxelData.h"
#include "Voxel/Voxels/Voxel.h"
#include "Voxel/Voxels/VoxelPlant.h"

FVoxelWorldSaveData FVoxelWorldSaveData::Empty = FVoxelWorldSaveData();

FVoxelItem FVoxelItem::Empty = FVoxelItem();
FVoxelItem FVoxelItem::Unknown = FVoxelItem(FPrimaryAssetId(FName("Voxel"), FName("DA_Voxel_Unknown")));

FVoxelItem::FVoxelItem(EVoxelType InVoxelType, bool bRefreshData) : FVoxelItem()
{
	VoxelType = InVoxelType;
	ID = UVoxelModuleBPLibrary::VoxelTypeToAssetID(InVoxelType);
	if(bRefreshData)
	{
		RefreshData();
	}
}

FVoxelItem::FVoxelItem(const FPrimaryAssetId& InID, bool bRefreshData) : FVoxelItem()
{
	ID = InID;
	if(bRefreshData)
	{
		RefreshData();
	}
}

FVoxelItem::FVoxelItem(const FVoxelSaveData& InSaveData) : FVoxelSaveData(InSaveData)
{
	ID = UVoxelModuleBPLibrary::VoxelTypeToAssetID(InSaveData.VoxelType);
	RefreshData();
}

bool FVoxelItem::IsValid(bool bNeedNotNull) const
{
	return Super::IsValid(bNeedNotNull) && !IsUnknown();
}

bool FVoxelItem::IsUnknown() const
{
	return *this == Unknown;
}

bool FVoxelItem::IsReplaceable(const FVoxelItem& InVoxelItem) const
{
	return !IsValid() || !InVoxelItem.IsValid() || !EqualType(static_cast<FAbilityItem>(InVoxelItem)) && GetData<UVoxelData>().Transparency == EVoxelTransparency::Transparent && InVoxelItem.GetData<UVoxelData>().Transparency != EVoxelTransparency::Transparent;
}

void FVoxelItem::RefreshData()
{
	*this = GetVoxel().ToSaveDataRef<FVoxelItem>(true);
}

FVoxelItem& FVoxelItem::GetMainItem() const
{
	if(Owner)
	{
		return Owner->GetVoxelItem(Index + GetData<UVoxelData>().PartIndex);
	}
	return FVoxelItem::Empty;
}

FVoxelItem& FVoxelItem::GetPartItem(FIndex InIndex) const
{
	if(Owner && GetData<UVoxelData>().HasPartData(InIndex))
	{
		return Owner->GetVoxelItem(Index + InIndex);
	}
	return FVoxelItem::Empty;
}

FVector FVoxelItem::GetRange() const
{
	return GetData<UVoxelData>().GetRange(Rotation, Scale);
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
