// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/VoxelModuleTypes.h"

#include "Asset/AssetModuleBPLibrary.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleBPLibrary.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Datas/VoxelData.h"
#include "Voxel/Voxels/Voxel.h"

FVoxelWorldSaveData FVoxelWorldSaveData::Empty = FVoxelWorldSaveData();

FVoxelItem FVoxelItem::Empty = FVoxelItem();
FVoxelItem FVoxelItem::Unknown = FVoxelItem(FPrimaryAssetId(FName("Voxel"), FName("DA_Voxel_Unknown")));

FVoxelItem::FVoxelItem(const FPrimaryAssetId& InID, FIndex InIndex, AVoxelChunk* InOwner, const FString& InData) : FVoxelItem()
{
	ID = InID;
	Index = InIndex;
	Owner = InOwner;
	Data = InData;
}

FVoxelItem::FVoxelItem(EVoxelType InVoxelType, FIndex InIndex, AVoxelChunk* InOwner, const FString& InData)
	: FVoxelItem(UVoxelModuleBPLibrary::VoxelTypeToAssetID(InVoxelType), InIndex, InOwner, InData)
{
}

FVoxelItem::FVoxelItem(const FString& InSaveData) : FVoxelItem()
{
	TArray<FString> DataStrs;
	InSaveData.ParseIntoArray(DataStrs, TEXT("|"));
	ID = UVoxelModuleBPLibrary::VoxelTypeToAssetID((EVoxelType)FCString::Atoi(*DataStrs[0]));
	Index = UVoxelModuleBPLibrary::NumberToVoxelIndex(FCString::Atoi(*DataStrs[1]));
	Angle = (ERightAngle)FCString::Atoi(*DataStrs[2]);
	if(DataStrs.IsValidIndex(3))
	{
		Data = *DataStrs[3];
	}
}

void FVoxelItem::OnGenerate(IVoxelAgentInterface* InAgent)
{
	if(bGenerated) return;
	
	bGenerated = true;
	if(Owner)
	{
		Owner->SpawnAuxiliary(*this);
	}
	GetVoxel().OnGenerate(InAgent);
}

void FVoxelItem::OnDestroy(IVoxelAgentInterface* InAgent)
{
	if(!bGenerated) return;
	
	GetVoxel().OnDestroy(InAgent);
	if(Owner)
	{
		Owner->DestroyAuxiliary(*this);
	}
	bGenerated = false;
}

void FVoxelItem::RefreshData(bool bOrigin)
{
	if(bOrigin && Owner)
	{
		FVoxelItem& OriginItem = Owner->GetVoxelItem(Index);
		OriginItem.RefreshData(false);
		*this = OriginItem;
		Owner->SetChanged(true);
	}
	else
	{
		Data = GetVoxel().ToData();
	}
}

void FVoxelItem::RefreshData(UVoxel& InVoxel, bool bOrigin)
{
	if(bOrigin && Owner)
	{
		FVoxelItem& OriginItem = Owner->GetVoxelItem(Index);
		OriginItem.RefreshData(InVoxel, false);
		*this = OriginItem;
		Owner->SetChanged(true);
	}
	else
	{
		Data = InVoxel.ToData();
	}
}

FString FVoxelItem::ToSaveData(bool bRefresh) const
{
	return FString::Printf(TEXT("%d|%d|%d|%s"), (int32)GetVoxelType(), UVoxelModuleBPLibrary::VoxelIndexToNumber(Index), (int32)Angle, !bRefresh ? *Data : *GetVoxel().ToData());
}

bool FVoxelItem::IsValid() const
{
	return Super::IsValid() && !IsUnknown();
}

bool FVoxelItem::IsUnknown() const
{
	return *this == Unknown;
}

bool FVoxelItem::IsReplaceable(const FVoxelItem& InVoxelItem) const
{
	return !IsValid() || (!InVoxelItem.IsValid() && GetVoxelType() != EVoxelType::Bedrock) || !EqualType(static_cast<FAbilityItem>(InVoxelItem)) && GetVoxelData().Transparency == EVoxelTransparency::Transparent && InVoxelItem.GetVoxelData().Transparency != EVoxelTransparency::Transparent;
}

FVoxelItem FVoxelItem::ReplaceID(const FPrimaryAssetId& InID) const
{
	FVoxelItem tmpItem = *this;
	tmpItem.ID = InID;
	return tmpItem;
}

FVoxelItem& FVoxelItem::GetMain() const
{
	if(Owner) return Owner->GetVoxelItem(Index - GetVoxelData().PartIndex);
	return FVoxelItem::Empty;
}

FVoxelItem& FVoxelItem::GetPart(FIndex InIndex) const
{
	if(Owner && GetVoxelData().HasPartData(InIndex))
	{
		return Owner->GetVoxelItem(Index + InIndex);
	}
	return FVoxelItem::Empty;
}

TArray<FVoxelItem> FVoxelItem::GetParts() const
{
	if(Owner)
	{
		TArray<FVoxelItem> Parts;
		for(auto Iter : GetVoxelData().PartDatas)
		{
			Parts.Add(Owner->GetVoxelItem(Index + Iter.Key));
		}
		return Parts;
	}
	return TArray<FVoxelItem>();
}

EVoxelType FVoxelItem::GetVoxelType() const
{
	return GetVoxelData().VoxelType;
}

FVector FVoxelItem::GetRange() const
{
	return GetVoxelData().GetRange(Angle);
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

UVoxelData& FVoxelItem::GetVoxelData(bool bLogWarning) const
{
	return UAssetModuleBPLibrary::LoadPrimaryAssetRef<UVoxelData>(ID, bLogWarning);
}

FVoxelHitResult::FVoxelHitResult(const FHitResult& InHitResult)
{
	if(AVoxelChunk* chunk = Cast<AVoxelChunk>(InHitResult.GetActor()))
	{
		VoxelItem = chunk->GetVoxelItem(chunk->LocationToIndex(InHitResult.ImpactPoint - AVoxelModule::Get()->GetWorldData().GetBlockSizedNormal(InHitResult.ImpactNormal, 0.01f)), true);
		Point = InHitResult.ImpactPoint;
		Normal = InHitResult.ImpactNormal;
	}
}

FVoxelHitResult::FVoxelHitResult(const FVoxelItem& InVoxelItem, FVector InPoint, FVector InNormal)
{
	VoxelItem = InVoxelItem;
	Point = InPoint;
	Normal = InNormal;
}

bool FVoxelHitResult::IsValid() const
{
	return VoxelItem.IsValid();
}

UVoxel& FVoxelHitResult::GetVoxel() const
{
	return VoxelItem.GetVoxel();
}

AVoxelChunk* FVoxelHitResult::GetChunk() const
{
	return VoxelItem.Owner;
}
