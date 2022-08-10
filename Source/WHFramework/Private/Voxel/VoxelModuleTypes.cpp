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

FVoxelItem::FVoxelItem(const FVoxelSaveData& InSaveData) : FAbilityItem(InSaveData)
{
	TArray<FString> data;
	InSaveData.StringData.ParseIntoArray(data, TEXT(";"));
	if(data.Num() >= 3)
	{
		ID = UVoxelModuleBPLibrary::VoxelTypeToAssetID((EVoxelType)FCString::Atoi(*data[0]));
		Index = FIndex(data[1]);
		Rotation.InitFromString(data[2]);
	}
	Owner = nullptr;
	Auxiliary = nullptr;
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
	*this = GetVoxel().ToSaveDataRef<FVoxelSaveData>(true);
}

FVoxelSaveData& FVoxelItem::ToSaveData(bool bRefresh) const
{
	if(!bRefresh)
	{
		static FVoxelSaveData VoxelSaveData;
		VoxelSaveData.Reset();
		VoxelSaveData = FVoxelSaveData(*this);
		VoxelSaveData.StringData = FString::Printf(TEXT("%d;%s;%s"), UVoxelModuleBPLibrary::AssetIDToVoxelType(ID), *Index.ToString(), *Rotation.ToString());
		return VoxelSaveData;
	}
	return GetVoxel().ToSaveDataRef<FVoxelItem>(true).ToSaveData(false);
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

EVoxelType FVoxelItem::GetVoxelType() const
{
	return UVoxelModuleBPLibrary::AssetIDToVoxelType(ID);
}

FVector FVoxelItem::GetRange() const
{
	return GetData<UVoxelData>().GetRange(Rotation);
}

FVector FVoxelItem::GetLocation(bool bWorldSpace) const
{
	if(Owner)
	{
		return Owner->IndexToLocation(Index, bWorldSpace);
	}
	return FVector::ZeroVector;
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
