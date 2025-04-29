// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/VoxelModuleStatics.h"

#include "Asset/AssetModuleStatics.h"
#include "Common/CommonStatics.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Voxels/Data/VoxelData.h"
#include "Voxel/Voxels/Voxel.h"

FPrimaryAssetId UVoxelModuleStatics::VoxelTypeToAssetID(EVoxelType InVoxelType)
{
	return FPrimaryAssetId(FName("Voxel"), *FString::Printf(TEXT("DA_%s"), *UCommonStatics::GetEnumAuthoredNameByValue(TEXT("/Script/WHFramework.EVoxelType"), (int32)InVoxelType)));
}

EVoxelTransparency UVoxelModuleStatics::VoxelNatureToTransparency(EVoxelNature InVoxelNature)
{
	switch (InVoxelNature)
	{
		case EVoxelNature::Solid:
		{
			return EVoxelTransparency::Solid;
		}
		case EVoxelNature::SemiSolid:
		case EVoxelNature::TransSolid:
		case EVoxelNature::Foliage:
		{
			return EVoxelTransparency::Semi;
		}
		case EVoxelNature::SmallSemiSolid:
		case EVoxelNature::Liquid:
		case EVoxelNature::SemiLiquid:
		case EVoxelNature::SemiFoliage:
		{
			return EVoxelTransparency::Trans;
		}
		default: break;
	}
	return EVoxelTransparency::Solid;
}

FIndex UVoxelModuleStatics::LocationToChunkIndex(FVector InLocation)
{
	return UVoxelModule::Get().LocationToChunkIndex(InLocation);
}

FVector UVoxelModuleStatics::ChunkIndexToLocation(FIndex InIndex)
{
	return UVoxelModule::Get().ChunkIndexToLocation(InIndex);
}

FIndex UVoxelModuleStatics::LocationToVoxelIndex(FVector InLocation)
{
	return UVoxelModule::Get().LocationToVoxelIndex(InLocation);
}

FVector UVoxelModuleStatics::VoxelIndexToLocation(FIndex InIndex)
{
	return UVoxelModule::Get().VoxelIndexToLocation(InIndex);
}

int64 UVoxelModuleStatics::VoxelIndexToNumber(FIndex InIndex, bool bWorldSpace)
{
	return UVoxelModule::Get().VoxelIndexToNumber(InIndex, bWorldSpace);
}

FIndex UVoxelModuleStatics::NumberToVoxelIndex(int64 InNumber, bool bWorldSpace)
{
	return UVoxelModule::Get().NumberToVoxelIndex(InNumber, bWorldSpace);
}

FIndex UVoxelModuleStatics::RightAngleToVoxelIndex(ERightAngle InAngle)
{
	switch(InAngle)
	{
		case ERightAngle::RA_0:		return FIndex(0, 0, 0);
		case ERightAngle::RA_90:	return FIndex(-1, 0, 0);
		case ERightAngle::RA_180:	return FIndex(-1, -1, 0);
		case ERightAngle::RA_270:	return FIndex(0, -1, 0);
		default:					return FIndex::ZeroIndex;
	}
}

float UVoxelModuleStatics::GetVoxelNoise1D(float InValue, bool bAbs, bool bUnsigned)
{
	return UVoxelModule::Get().GetVoxelNoise1D(InValue, bAbs, bUnsigned);
}

float UVoxelModuleStatics::GetVoxelNoise2D(FVector2D InLocation, bool bAbs, bool bUnsigned)
{
	return UVoxelModule::Get().GetVoxelNoise2D(InLocation, bAbs, bUnsigned);
}

float UVoxelModuleStatics::GetVoxelNoise3D(FVector InLocation, bool bAbs, bool bUnsigned)
{
	return UVoxelModule::Get().GetVoxelNoise3D(InLocation, bAbs, bUnsigned);
}

FVoxelWorldSaveData& UVoxelModuleStatics::GetVoxelWorldData()
{
	return UVoxelModule::Get().GetWorldData();
}

EVoxelWorldMode UVoxelModuleStatics::GetVoxelWorldMode()
{
	return UVoxelModule::Get().GetWorldMode();
}

void UVoxelModuleStatics::SetVoxelWorldMode(EVoxelWorldMode InWorldMode)
{
	UVoxelModule::Get().SetWorldMode(InWorldMode);
}

EVoxelWorldState UVoxelModuleStatics::GetVoxelWorldState()
{
	return UVoxelModule::Get().GetWorldState();
}

FVoxelWorldBasicSaveData UVoxelModuleStatics::GetVoxelWorldBasicData()
{
	return UVoxelModule::Get().GetWorldBasicData();
}

void UVoxelModuleStatics::LoadVoxelPrefabData(const FVoxelPrefabSaveData& InPrefabData)
{
	return UVoxelModule::Get().LoadPrefabData(InPrefabData);
}

FVoxelPrefabSaveData UVoxelModuleStatics::GetVoxelPrefabData()
{
	return UVoxelModule::Get().GetPrefabData();
}

const FVoxelTopography& UVoxelModuleStatics::GetTopographyByIndex(FIndex InIndex)
{
	return UVoxelModule::Get().GetTopographyByIndex(InIndex);
}

const FVoxelTopography& UVoxelModuleStatics::GetTopographyByLocation(FVector InLocation)
{
	return UVoxelModule::Get().GetTopographyByLocation(InLocation);
}

void UVoxelModuleStatics::SetTopographyByIndex(FIndex InIndex, const FVoxelTopography& InTopography)
{
	UVoxelModule::Get().SetTopographyByIndex(InIndex, InTopography);
}

void UVoxelModuleStatics::SetTopographyByLocation(FVector InLocation, const FVoxelTopography& InTopography)
{
	UVoxelModule::Get().SetTopographyByLocation(InLocation, InTopography);
}

float UVoxelModuleStatics::GetWorldGeneratePercent()
{
	return UVoxelModule::Get().GetWorldGeneratePercent();
}

FBox UVoxelModuleStatics::GetWorldBounds(float InRadius, float InHalfHeight)
{
	return UVoxelModule::Get().GetWorldBounds(InRadius, InHalfHeight);
}

int32 UVoxelModuleStatics::GetChunkNum(bool bNeedGenerated)
{
	return UVoxelModule::Get().GetChunkNum(bNeedGenerated);
}

bool UVoxelModuleStatics::IsChunkGenerated(FIndex InIndex)
{
	return UVoxelModule::Get().IsChunkGenerated(InIndex);
}

FVoxelChunkQueues UVoxelModuleStatics::GetChunkQueues(EVoxelWorldState InWorldState)
{
	return UVoxelModule::Get().GetChunkQueues(InWorldState);
}

UVoxelGenerator* UVoxelModuleStatics::GetVoxelGenerator(const TSubclassOf<UVoxelGenerator>& InClass)
{
	return UVoxelModule::Get().GetVoxelGenerator(InClass);
}

AVoxelChunk* UVoxelModuleStatics::GetChunkByIndex(FIndex InIndex)
{
	return UVoxelModule::Get().GetChunkByIndex(InIndex);
}

AVoxelChunk* UVoxelModuleStatics::GetChunkByLocation(FVector InLocation)
{
	return UVoxelModule::Get().GetChunkByLocation(InLocation);
}

FVoxelItem& UVoxelModuleStatics::GetVoxelByIndex(FIndex InIndex)
{
	return UVoxelModule::Get().GetVoxelByIndex(InIndex);
}

FVoxelItem& UVoxelModuleStatics::GetVoxelByLocation(FVector InLocation)
{
	return UVoxelModule::Get().GetVoxelByLocation(InLocation);
}

UVoxel& UVoxelModuleStatics::GetVoxel(EVoxelType InVoxelType)
{
	return GetVoxel(VoxelTypeToAssetID(InVoxelType));
}

UVoxel& UVoxelModuleStatics::GetVoxel(const FPrimaryAssetId& InVoxelID)
{
	const UVoxelData& VoxelData = UAssetModuleStatics::LoadPrimaryAssetRef<UVoxelData>(InVoxelID);
	if(VoxelData.IsValid())
	{
		UVoxel& Voxel = UReferencePoolModuleStatics::GetReference<UVoxel>(true, VoxelData.VoxelClass.Get() ? VoxelData.VoxelClass.Get() : UVoxel::StaticClass());
		Voxel.SetItem(InVoxelID);
		return Voxel;
	}
	return UReferencePoolModuleStatics::GetReference<UVoxel>(true);
}

UVoxel& UVoxelModuleStatics::GetVoxel(const FVoxelItem& InVoxelItem)
{
	if(InVoxelItem.IsEmpty()) return UVoxel::GetEmpty();
	if(InVoxelItem.IsUnknown()) return UVoxel::GetUnknown();
	UVoxel& Voxel = GetVoxel(InVoxelItem.ID);
	Voxel.SetItem(InVoxelItem);
	if(!InVoxelItem.Data.IsEmpty())
	{
		Voxel.LoadData(InVoxelItem.Data);
	}
	return Voxel;
}

bool UVoxelModuleStatics::VoxelRaycastSinge(FVector InRayStart, FVector InRayEnd, const TArray<AActor*>& InIgnoreActors, FVoxelHitResult& OutHitResult)
{
	return UVoxelModule::Get().VoxelRaycastSinge(InRayStart, InRayEnd, InIgnoreActors, OutHitResult);
}

bool UVoxelModuleStatics::VoxelRaycastSinge(EVoxelRaycastType InRaycastType, float InDistance, const TArray<AActor*>& InIgnoreActors, FVoxelHitResult& OutHitResult)
{
	return UVoxelModule::Get().VoxelRaycastSinge(InRaycastType, InDistance, InIgnoreActors, OutHitResult);
}

bool UVoxelModuleStatics::VoxelItemTraceSingle(const FVoxelItem& InVoxelItem, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult)
{
	return UVoxelModule::Get().VoxelItemTraceSingle(InVoxelItem, InIgnoreActors, OutHitResult);
}

bool UVoxelModuleStatics::VoxelAgentTraceSingle(FIndex InChunkIndex, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult, bool bSnapToBlock, int32 InMaxCount, bool bFromCenter, bool bForce)
{
	return UVoxelModule::Get().VoxelAgentTraceSingle(InChunkIndex, InRadius, InHalfHeight, InIgnoreActors, OutHitResult, bSnapToBlock, InMaxCount, bFromCenter, bForce);
}

bool UVoxelModuleStatics::VoxelAgentTraceSingle(FVector InLocation, FVector2D InRange, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult, bool bSnapToBlock, int32 InMaxCount, bool bFromCenter, bool bForce)
{
	return UVoxelModule::Get().VoxelAgentTraceSingle(InLocation, InRange, InRadius, InHalfHeight, InIgnoreActors, OutHitResult, bSnapToBlock, InMaxCount, bFromCenter, bForce);
}

bool UVoxelModuleStatics::VoxelAgentTraceSingle(FVector InRayStart, FVector InRayEnd, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult, bool bCheckVoxel)
{
	return UVoxelModule::Get().VoxelAgentTraceSingle(InRayStart, InRayEnd, InRadius, InHalfHeight, InIgnoreActors, OutHitResult, bCheckVoxel);
}
