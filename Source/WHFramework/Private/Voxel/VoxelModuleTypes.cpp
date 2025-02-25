// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/VoxelModuleTypes.h"

#include "Asset/AssetModuleStatics.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Datas/VoxelData.h"
#include "Voxel/Voxels/Voxel.h"

FVoxelItem FVoxelItem::Empty = FVoxelItem(FPrimaryAssetId(FName("Voxel"), FName("DA_Empty")));
FVoxelItem FVoxelItem::Unknown = FVoxelItem(FPrimaryAssetId(FName("Voxel"), FName("DA_Unknown")));

FVoxelItem::FVoxelItem(const FPrimaryAssetId& InID, FIndex InIndex, AVoxelChunk* InOwner, const FString& InData) : FVoxelItem()
{
	ID = InID;
	Index = InIndex;
	Owner = InOwner;
	Data = InData;
}

FVoxelItem::FVoxelItem(EVoxelType InVoxelType, FIndex InIndex, AVoxelChunk* InOwner, const FString& InData)
	: FVoxelItem(UVoxelModuleStatics::VoxelTypeToAssetID(InVoxelType), InIndex, InOwner, InData)
{
}

FVoxelItem::FVoxelItem(const FString& InSaveData) : FVoxelItem()
{
	TArray<FString> DataStrs;
	InSaveData.ParseIntoArray(DataStrs, TEXT(";"));
	ID = UVoxelModuleStatics::VoxelTypeToAssetID((EVoxelType)FCString::Atoi(*DataStrs[0]));
	Index = UVoxelModuleStatics::NumberToVoxelIndex(FCString::Atoi(*DataStrs[1]));
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
		FVoxelItem& OriginItem = Owner->GetVoxel(Index);
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
		FVoxelItem& OriginItem = Owner->GetVoxel(Index);
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
	const FString _Data = !bRefresh ? *Data : *GetVoxel().ToData();
	return FString::Printf(TEXT("%d;%d;%d%s"), (int32)GetVoxelType(), UVoxelModuleStatics::VoxelIndexToNumber(Index), (int32)Angle, !_Data.IsEmpty() ? *FString::Printf(TEXT(";%s"), *_Data) : TEXT(""));
}

bool FVoxelItem::IsValid() const
{
	return Super::IsValid() && !EqualID(Empty) && !EqualID(Unknown);
}

bool FVoxelItem::IsEmpty() const
{
	return *this == Empty;
}

bool FVoxelItem::IsUnknown() const
{
	return *this == Unknown;
}

bool FVoxelItem::IsReplaceable(const FVoxelItem& InVoxelItem) const
{
	return !IsValid() || (!InVoxelItem.IsValid() && GetVoxelType() != EVoxelType::Bedrock) || !Match(static_cast<FAbilityItem>(InVoxelItem)) && GetVoxelData().GetTransparency() == EVoxelTransparency::Trans && InVoxelItem.GetVoxelData().GetTransparency() != EVoxelTransparency::Trans;
}

FVoxelItem FVoxelItem::ReplaceID(const FPrimaryAssetId& InID) const
{
	FVoxelItem _Item = *this;
	_Item.ID = InID;
	return _Item;
}

FVoxelItem& FVoxelItem::GetMain() const
{
	if(Owner) return Owner->GetVoxelComplex(Index - GetVoxelData().PartIndex);
	return FVoxelItem::Empty;
}

FVoxelItem& FVoxelItem::GetPart(FIndex InIndex) const
{
	if(Owner && GetVoxelData().HasPartData(InIndex))
	{
		return Owner->GetVoxelComplex(Index + InIndex);
	}
	return FVoxelItem::Empty;
}

TArray<FVoxelItem> FVoxelItem::GetParts() const
{
	if(Owner)
	{
		TArray<FVoxelItem> Parts;
		for(auto& Iter : GetVoxelData().PartDatas)
		{
			Parts.Add(Owner->GetVoxelComplex(Index + Iter.Key));
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
	return UVoxelModuleStatics::GetVoxel(*this);
}

AVoxelChunk* FVoxelItem::GetOwner() const
{
	if(Owner) return Owner;
	return UVoxelModuleStatics::FindChunkByIndex(Index);
}

UVoxelData& FVoxelItem::GetVoxelData(bool bEnsured) const
{
	return UAssetModuleStatics::LoadPrimaryAssetRef<UVoxelData>(ID, bEnsured);
}

FVoxelHitResult::FVoxelHitResult(const FHitResult& InHitResult)
{
	if(AVoxelChunk* InChunk = Cast<AVoxelChunk>(InHitResult.GetActor()))
	{
		VoxelItem = InChunk->GetVoxelComplex(InChunk->LocationToIndex(InHitResult.ImpactPoint - UVoxelModule::Get().GetWorldData().GetBlockSizedNormal(InHitResult.ImpactNormal, 0.01f)), true);
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

FVoxelTopography::FVoxelTopography(const FString& InSaveData)
{
	TArray<FString> DataStrs;
	InSaveData.ParseIntoArray(DataStrs, TEXT(";"));
	Index = UVoxelModuleStatics::NumberToVoxelIndex(FCString::Atoi(*DataStrs[0]));
	Height = FCString::Atoi(*DataStrs[1]);
	Temperature = FCString::Atof(*DataStrs[2]);
	Humidity = FCString::Atof(*DataStrs[3]);
	BiomeType = (EVoxelBiomeType)FCString::Atoi(*DataStrs[4]);
}

FString FVoxelTopography::ToSaveData() const
{
	return FString::Printf(TEXT("%d;%d;%f;%f%d"), UVoxelModuleStatics::VoxelIndexToNumber(Index), Height, Temperature, Humidity, (int32)BiomeType);
}
