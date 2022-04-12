// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/VoxelModuleBPLibrary.h"

#include "Asset/AssetModuleBPLibrary.h"
#include "Global/GlobalBPLibrary.h"
#include "Voxel/VoxelModule.h"

FPrimaryAssetId UVoxelModuleBPLibrary::GetAssetIDByVoxelType(EVoxelType InVoxelType)
{
	return FPrimaryAssetId(FName("Voxel"), *FString::Printf(TEXT("DA_Voxel_%s"), *UGlobalBPLibrary::GetEnumValueAuthoredName(TEXT("EVoxelType"), (int32)InVoxelType)));
}

FIndex UVoxelModuleBPLibrary::LocationToChunkIndex(FVector InLocation, bool bIgnoreZ /*= false*/)
{
	FIndex chunkIndex = FIndex(FMath::FloorToInt(InLocation.X / AVoxelModule::GetWorldData()->GetChunkLength()),
		FMath::FloorToInt(InLocation.Y / AVoxelModule::GetWorldData()->GetChunkLength()),
		bIgnoreZ ? 0 : FMath::FloorToInt(InLocation.Z / AVoxelModule::GetWorldData()->GetChunkLength()));
	return chunkIndex;
}

FVector UVoxelModuleBPLibrary::ChunkIndexToLocation(FIndex InIndex)
{
	return InIndex.ToVector() * AVoxelModule::GetWorldData()->GetChunkLength();
}
