// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/VoxelModuleTypes.h"

#include "Voxel/VoxelModuleBPLibrary.h"
#include "Voxel/Voxels/Voxel.h"
#include "Voxel/Voxels/VoxelPlant.h"

FVoxelWorldSaveData FVoxelWorldSaveData::Empty = FVoxelWorldSaveData();

FVoxelItem FVoxelItem::EmptyVoxel = FVoxelItem(FPrimaryAssetId(FName("Voxel"), FName("DA_Voxel_Empty")));

FVoxelItem FVoxelItem::UnknownVoxel = FVoxelItem(FPrimaryAssetId(FName("Voxel"), FName("DA_Voxel_Unknown")));

FVoxelItem::FVoxelItem(EVoxelType InVoxelType)
{
	ID = UVoxelModuleBPLibrary::GetAssetIDByVoxelType(InVoxelType);
	Index = FIndex::ZeroIndex;
	Rotation = FRotator::ZeroRotator;
	Scale = FVector::OneVector;
	Params = TMap<FName, FParameter>();
	Owner = nullptr;
	Auxiliary = nullptr;
}

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

void FVoxelWorldSaveData::Initialize()
{
	if(WorldSeed == 0)
	{
		WorldSeed = FMath::Rand();
	}
}
