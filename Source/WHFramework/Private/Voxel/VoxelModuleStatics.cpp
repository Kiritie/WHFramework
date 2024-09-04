// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/VoxelModuleStatics.h"

#include "Asset/AssetModuleStatics.h"
#include "Common/CommonStatics.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Datas/VoxelData.h"
#include "Voxel/Voxels/Voxel.h"

FPrimaryAssetId UVoxelModuleStatics::VoxelTypeToAssetID(EVoxelType InVoxelType)
{
	if(InVoxelType == EVoxelType::Empty) FPrimaryAssetId();

	return FPrimaryAssetId(FName("Voxel"), *FString::Printf(TEXT("DA_Voxel_%s"), *UCommonStatics::GetEnumValueAuthoredName(TEXT("/Script/WHFramework.EVoxelType"), (int32)InVoxelType)));
}

FVoxelWorldSaveData& UVoxelModuleStatics::GetWorldData()
{
	return UVoxelModule::Get().GetWorldData();
}

EVoxelWorldMode UVoxelModuleStatics::GetWorldMode()
{
	return UVoxelModule::Get().GetWorldMode();
}

EVoxelWorldState UVoxelModuleStatics::GetWorldState()
{
	return UVoxelModule::Get().GetWorldState();
}

FVoxelWorldBasicSaveData UVoxelModuleStatics::GetWorldBasicData()
{
	return UVoxelModule::Get().GetWorldBasicData();
}

FIndex UVoxelModuleStatics::LocationToChunkIndex(FVector InLocation, bool bIgnoreZ /*= false*/)
{
	return UVoxelModule::Get().LocationToChunkIndex(InLocation, bIgnoreZ);
}

FVector UVoxelModuleStatics::ChunkIndexToLocation(FIndex InIndex)
{
	return UVoxelModule::Get().ChunkIndexToLocation(InIndex);
}

FIndex UVoxelModuleStatics::LocationToVoxelIndex(FVector InLocation, bool bIgnoreZ)
{
	return UVoxelModule::Get().LocationToVoxelIndex(InLocation, bIgnoreZ);
}

FVector UVoxelModuleStatics::VoxelIndexToLocation(FIndex InIndex)
{
	return UVoxelModule::Get().VoxelIndexToLocation(InIndex);
}

int32 UVoxelModuleStatics::VoxelIndexToNumber(FIndex InIndex)
{
	return UVoxelModule::Get().VoxelIndexToNumber(InIndex);
}

FIndex UVoxelModuleStatics::NumberToVoxelIndex(int32 InNumber)
{
	return UVoxelModule::Get().NumberToVoxelIndex(InNumber);
}

AVoxelChunk* UVoxelModuleStatics::FindChunkByIndex(FIndex InIndex)
{
	return UVoxelModule::Get().FindChunkByIndex(InIndex);
}

AVoxelChunk* UVoxelModuleStatics::FindChunkByLocation(FVector InLocation)
{
	return UVoxelModule::Get().FindChunkByLocation(InLocation);
}

FVoxelItem& UVoxelModuleStatics::FindVoxelByIndex(FIndex InIndex)
{
	return UVoxelModule::Get().FindVoxelByIndex(InIndex);
}

FVoxelItem& UVoxelModuleStatics::FindVoxelByLocation(FVector InLocation)
{
	return UVoxelModule::Get().FindVoxelByLocation(InLocation);
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
		const TSubclassOf<UVoxel> VoxelClass = VoxelData.VoxelClass.Get() ? VoxelData.VoxelClass.Get() : UVoxel::StaticClass();
		UVoxel& Voxel = UReferencePoolModuleStatics::GetReference<UVoxel>(true, VoxelClass);
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

EVoxelType UVoxelModuleStatics::GetNoiseVoxelType(FIndex InIndex)
{
	return UVoxelModule::Get().GetNoiseVoxelType(InIndex);
}

EVoxelType UVoxelModuleStatics::GetRandomVoxelType(FIndex InIndex)
{
	return UVoxelModule::Get().GetRandomVoxelType(InIndex);
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

bool UVoxelModuleStatics::VoxelAgentTraceSingle(FIndex InChunkIndex, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult, bool bSnapToBlock, int32 InMaxCount, bool bFromCenter)
{
	return UVoxelModule::Get().VoxelAgentTraceSingle(InChunkIndex, InRadius, InHalfHeight, InIgnoreActors, OutHitResult, bSnapToBlock, InMaxCount, bFromCenter);
}

bool UVoxelModuleStatics::VoxelAgentTraceSingle(FVector InLocation, FVector2D InRange, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult, bool bSnapToBlock, int32 InMaxCount, bool bFromCenter)
{
	return UVoxelModule::Get().VoxelAgentTraceSingle(InLocation, InRange, InRadius, InHalfHeight, InIgnoreActors, OutHitResult, bSnapToBlock, InMaxCount, bFromCenter);
}

bool UVoxelModuleStatics::VoxelAgentTraceSingle(FVector InRayStart, FVector InRayEnd, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult)
{
	return UVoxelModule::Get().VoxelAgentTraceSingle(InRayStart, InRayEnd, InRadius, InHalfHeight, InIgnoreActors, OutHitResult);
}
