// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/VoxelModuleTypes.h"

#include "Asset/AssetModuleStatics.h"
#include "Math/MathHelper.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Voxels/Data/VoxelData.h"
#include "Voxel/Voxels/Voxel.h"

FVoxelItem FVoxelItem::Empty = FVoxelItem(FPrimaryAssetId(FName("Voxel"), FName("DA_Empty")));
FVoxelItem FVoxelItem::Unknown = FVoxelItem(FPrimaryAssetId(FName("Voxel"), FName("DA_Unknown")));

FVoxelItem::FVoxelItem(const FPrimaryAssetId& InID, FIndex InIndex, AVoxelChunk* InOwner, const FString& InData) : FVoxelItem()
{
	ID = InID;
	Index = InIndex;
	Chunk = InOwner;
	Data = InData;
}

FVoxelItem::FVoxelItem(EVoxelType InVoxelType, FIndex InIndex, AVoxelChunk* InOwner, const FString& InData)
	: FVoxelItem(UVoxelModuleStatics::VoxelTypeToAssetID(InVoxelType), InIndex, InOwner, InData)
{
}

FVoxelItem::FVoxelItem(const FString& InSaveData, bool bWorldSpace) : FVoxelItem()
{
	TArray<FString> DataStrs;
	if(InSaveData.ParseIntoArray(DataStrs, TEXT(";")) >= 3)
	{
		int32 TempIndex = 0;
		ID = UVoxelModuleStatics::VoxelTypeToAssetID((EVoxelType)FCString::Atoi(*DataStrs[TempIndex++]));
		Index = UVoxelModuleStatics::NumberToVoxelIndex(FCString::Atoi64(*DataStrs[TempIndex++]), bWorldSpace);
		Angle = (ERightAngle)FCString::Atoi(*DataStrs[TempIndex++]);
		if(DataStrs.Num() > 4)
		{
			Durability = FCString::Atof(*DataStrs[TempIndex++]);
		}
		if(DataStrs.IsValidIndex(TempIndex))
		{
			Data = *DataStrs[TempIndex];
		}
	}
}

void FVoxelItem::OnGenerate(IVoxelAgentInterface* InAgent)
{
	if(bGenerated) return;
	
	bGenerated = true;
	if(Chunk)
	{
		Chunk->SpawnAuxiliary(*this);
	}
	GetVoxel().OnGenerate(InAgent);
}

void FVoxelItem::OnDestroy(IVoxelAgentInterface* InAgent)
{
	if(!bGenerated) return;
	
	GetVoxel().OnDestroy(InAgent);
	if(Chunk)
	{
		Chunk->DestroyAuxiliary(*this);
	}
	bGenerated = false;
}

void FVoxelItem::RefreshData(bool bOrigin)
{
	if(bOrigin && Chunk)
	{
		FVoxelItem& OriginItem = Chunk->GetVoxel(Index);
		OriginItem.RefreshData(false);
		*this = OriginItem;
		Chunk->SetChanged(true);
	}
	else
	{
		Data = GetVoxel().ToData();
	}
}

void FVoxelItem::RefreshData(UVoxel& InVoxel, bool bOrigin)
{
	if(bOrigin && Chunk)
	{
		FVoxelItem& OriginItem = Chunk->GetVoxel(Index);
		OriginItem.RefreshData(InVoxel, false);
		*this = OriginItem;
		Chunk->SetChanged(true);
	}
	else
	{
		Data = InVoxel.ToData();
	}
}

FString FVoxelItem::ToSaveData(bool bWorldSpace, bool bRefresh) const
{
	const FString _Data = !bRefresh ? *Data : *GetVoxel().ToData();
	return FString::Printf(TEXT("%d;%lld;%d%s;%s"), (int32)GetVoxelType(), UVoxelModuleStatics::VoxelIndexToNumber(GetIndex(bWorldSpace), bWorldSpace), (int32)Angle, !FMath::IsNearlyEqual(Durability, 1.f) ? *FString::Printf(TEXT(";%f"), Durability) : TEXT(""), !_Data.IsEmpty() ? *FString::Printf(TEXT(";%s"), *_Data) : TEXT(""));
}

bool FVoxelItem::IsValid() const
{
	return ID.IsValid() && !Equal(Empty) && !Equal(Unknown);
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
	return !IsValid() || (!InVoxelItem.IsValid() && GetVoxelType() != EVoxelType::Bedrock) || !Equal(InVoxelItem) && GetVoxelData().GetTransparency() == EVoxelTransparency::Trans && InVoxelItem.GetVoxelData().GetTransparency() != EVoxelTransparency::Trans;
}

FVoxelItem FVoxelItem::ReplaceID(const FPrimaryAssetId& InID) const
{
	FVoxelItem _Item = *this;
	_Item.ID = InID;
	return _Item;
}

bool FVoxelItem::IsMain() const
{
	return GetVoxelData().IsMainPart();
}

FVoxelItem& FVoxelItem::GetMain() const
{
	if(Chunk) return Chunk->GetVoxelComplex(Index - FMathHelper::RotateIndex(GetVoxelData().PartIndex, Angle));
	return FVoxelItem::Empty;
}

FVoxelItem& FVoxelItem::GetPart(FIndex InIndex) const
{
	if(Chunk && GetVoxelData().HasPartData(InIndex))
	{
		return Chunk->GetVoxelComplex(Index + InIndex);
	}
	return FVoxelItem::Empty;
}

TArray<FVoxelItem> FVoxelItem::GetParts() const
{
	if(Chunk)
	{
		TArray<FVoxelItem> Parts;
		for(auto Iter : GetVoxelData().PartDatas)
		{
			Parts.Add(Chunk->GetVoxelComplex(Index + FMathHelper::RotateIndex(Iter->PartIndex, Angle)));
		}
		return Parts;
	}
	return TArray<FVoxelItem>();
}

EVoxelType FVoxelItem::GetVoxelType() const
{
	return GetVoxelData().VoxelType;
}

FVector FVoxelItem::GetRange(bool bIncludeAngle, bool bIncludeDirection) const
{
	return GetVoxelData().GetRange(bIncludeAngle ? Angle : ERightAngle::RA_0, bIncludeDirection);
}

FIndex FVoxelItem::GetIndex(bool bWorldSpace) const
{
	if(Chunk && bWorldSpace)
	{
		return Chunk->LocalIndexToWorld(Index);
	}
	return Index;
}

FVector FVoxelItem::GetLocation(bool bWorldSpace) const
{
	if(Chunk)
	{
		return Chunk->IndexToLocation(Index, bWorldSpace);
	}
	return Index.ToVector() * UVoxelModule::Get().GetWorldData().BlockSize;
}

UVoxelData& FVoxelItem::GetData(bool bEnsured) const
{
	return UAssetModuleStatics::LoadPrimaryAssetRef<UVoxelData>(ID, bEnsured);
}

UVoxel& FVoxelItem::GetVoxel() const
{
	return UVoxelModuleStatics::GetVoxel(*this);
}

UVoxelData& FVoxelItem::GetVoxelData(bool bEnsured) const
{
	return UAssetModuleStatics::LoadPrimaryAssetRef<UVoxelData>(ID, bEnsured);
}

FVoxelHitResult::FVoxelHitResult(const FHitResult& InHitResult)
{
	if(AVoxelChunk* InChunk = Cast<AVoxelChunk>(InHitResult.GetActor()))
	{
		const FVoxelItem _VoxelItem = InChunk->GetVoxelComplex(InChunk->LocationToIndex(InHitResult.ImpactPoint - UVoxelModule::Get().GetWorldData().GetBlockSizedNormal(InHitResult.ImpactNormal, 0.01f)), true);
		VoxelItem = _VoxelItem.IsMain() ? _VoxelItem : _VoxelItem.GetMain();
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
	return VoxelItem.Chunk;
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
	return FString::Printf(TEXT("%lld;%d;%f;%f%d"), UVoxelModuleStatics::VoxelIndexToNumber(Index), Height, Temperature, Humidity, (int32)BiomeType);
}
