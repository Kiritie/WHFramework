// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/VoxelModuleBPLibrary.h"

#include "Asset/AssetModuleBPLibrary.h"
#include "Global/GlobalBPLibrary.h"
#include "Voxel/VoxelModule.h"

FPrimaryAssetId UVoxelModuleBPLibrary::GetAssetIDByVoxelType(EVoxelType InVoxelType)
{
	return FPrimaryAssetId(FName("Voxel"), *FString::Printf(TEXT("DA_Voxel_%s"), *UGlobalBPLibrary::GetEnumValueAuthoredName(TEXT("EVoxelType"), (int32)InVoxelType)));
}

FVoxelWorldSaveData& UVoxelModuleBPLibrary::GetWorldData()
{
	if(AVoxelModule* VoxelModule = AMainModule::GetModuleByClass<AVoxelModule>())
	{
		return VoxelModule->GetWorldData();
	}
	return FVoxelWorldSaveData::Empty;
}

FIndex UVoxelModuleBPLibrary::LocationToChunkIndex(FVector InLocation, bool bIgnoreZ /*= false*/)
{
	FIndex chunkIndex = FIndex(FMath::FloorToInt(InLocation.X / UVoxelModuleBPLibrary::GetWorldData().GetChunkLength()),
		FMath::FloorToInt(InLocation.Y / UVoxelModuleBPLibrary::GetWorldData().GetChunkLength()),
		bIgnoreZ ? 0 : FMath::FloorToInt(InLocation.Z / UVoxelModuleBPLibrary::GetWorldData().GetChunkLength()));
	return chunkIndex;
}

FVector UVoxelModuleBPLibrary::ChunkIndexToLocation(FIndex InIndex)
{
	return InIndex.ToVector() * UVoxelModuleBPLibrary::GetWorldData().GetChunkLength();
}
