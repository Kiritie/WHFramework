// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/VoxelModuleTypes.h"

#include "Asset/AssetModuleBPLibrary.h"
#include "Voxel/VoxelModule.h"
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
	RefreshData();
	TArray<FString> DataStrs;
	InSaveData.VoxelData.ParseIntoArray(DataStrs, TEXT(";"));
	if(DataStrs.Num() >= 3)
	{
		ID = UVoxelModuleBPLibrary::VoxelTypeToAssetID((EVoxelType)FCString::Atoi(*DataStrs[0]));
		Index = FIndex(DataStrs[1]);
		Angle = (ERightAngle)FCString::Atoi(*DataStrs[2]);
	}
	Owner = nullptr;
	Auxiliary = nullptr;
	bGenerated = false;
}

void FVoxelItem::OnGenerate(IVoxelAgentInterface* InAgent)
{
	if(bGenerated) return;
	
	bGenerated = true;
	if(Owner)
	{
		Auxiliary = Owner->SpawnAuxiliary(Index);
	}
	GetVoxel().OnGenerate(InAgent);
}

void FVoxelItem::OnDestroy(IVoxelAgentInterface* InAgent)
{
	if(!bGenerated) return;
	
	GetVoxel().OnDestroy(InAgent);
	if(Owner)
	{
		Owner->DestroyAuxiliary(Index);
		Auxiliary = nullptr;
	}
	bGenerated = false;
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
	return !IsValid() || (!InVoxelItem.IsValid() && GetVoxelType() != EVoxelType::Bedrock) || !EqualType(static_cast<FAbilityItem>(InVoxelItem)) && GetVoxelData().Transparency == EVoxelTransparency::Transparent && InVoxelItem.GetVoxelData().Transparency != EVoxelTransparency::Transparent;
}

void FVoxelItem::RefreshData(UVoxel* InVoxel)
{
	*this = (InVoxel ? *InVoxel : GetVoxel()).GetSaveDataRef<FVoxelItem>(true);
}

FVoxelSaveData FVoxelItem::ToSaveData(bool bRefresh) const
{
	if(!bRefresh)
	{
		FVoxelSaveData VoxelSaveData = FVoxelSaveData(*this);
		VoxelSaveData.VoxelData = FString::Printf(TEXT("%d;%s;%d"), GetVoxelType(), *Index.ToString(), Angle);
		return VoxelSaveData;
	}
	return GetVoxel().GetSaveDataRef<FVoxelItem>().ToSaveData(false);
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
