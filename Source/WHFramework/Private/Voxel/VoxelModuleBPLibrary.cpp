// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/VoxelModuleBPLibrary.h"

#include "Asset/AssetModuleBPLibrary.h"
#include "Global/GlobalBPLibrary.h"
#include "ObjectPool/ObjectPoolModuleBPLibrary.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Datas/VoxelData.h"
#include "Voxel/Voxels/Voxel.h"

FPrimaryAssetId UVoxelModuleBPLibrary::VoxelTypeToAssetID(EVoxelType InVoxelType)
{
	return FPrimaryAssetId(FName("Voxel"), *FString::Printf(TEXT("DA_Voxel_%s"), *UGlobalBPLibrary::GetEnumValueAuthoredName(TEXT("EVoxelType"), (int32)InVoxelType)));
}

EVoxelType UVoxelModuleBPLibrary::AssetIDToVoxelType(FPrimaryAssetId InAssetID)
{
	return (EVoxelType)UGlobalBPLibrary::GetEnumIndexByValueName(TEXT("EVoxelType"), InAssetID.PrimaryAssetName.ToString().Mid(9));
}

FVoxelWorldSaveData& UVoxelModuleBPLibrary::GetWorldData()
{
	if(AVoxelModule* VoxelModule = AMainModule::GetModuleByClass<AVoxelModule>())
	{
		return VoxelModule->GetWorldData();
	}
	return FVoxelWorldSaveData::Empty;
}

EVoxelWorldMode UVoxelModuleBPLibrary::GetWorldMode()
{
	if(AVoxelModule* VoxelModule = AMainModule::GetModuleByClass<AVoxelModule>())
	{
		return VoxelModule->GetWorldMode();
	}
	return EVoxelWorldMode::None;
}

EVoxelWorldState UVoxelModuleBPLibrary::GetWorldState()
{
	if(AVoxelModule* VoxelModule = AMainModule::GetModuleByClass<AVoxelModule>())
	{
		return VoxelModule->GetWorldState();
	}
	return EVoxelWorldState::None;
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

UVoxel& UVoxelModuleBPLibrary::GetVoxel(EVoxelType InVoxelType)
{
	return GetVoxel(UVoxelModuleBPLibrary::VoxelTypeToAssetID(InVoxelType));
}

UVoxel& UVoxelModuleBPLibrary::GetVoxel(const FPrimaryAssetId& InVoxelID)
{
	const UVoxelData& voxelData = UAssetModuleBPLibrary::LoadPrimaryAssetRef<UVoxelData>(InVoxelID);
	if(voxelData.IsValid())
	{
		const TSubclassOf<UVoxel> tmpClass = voxelData.VoxelClass ? voxelData.VoxelClass : TSubclassOf<UVoxel>(StaticClass());
		UVoxel& voxel = UReferencePoolModuleBPLibrary::GetReference<UVoxel>(tmpClass);
		voxel.SetID(InVoxelID);
		return voxel;
	}
	return UReferencePoolModuleBPLibrary::GetReference<UVoxel>();;
}

UVoxel& UVoxelModuleBPLibrary::GetVoxel(const FVoxelItem& InVoxelItem)
{
	UVoxel& voxel = GetVoxel(InVoxelItem.ID);
	voxel.LoadSaveData(&const_cast<FVoxelItem&>(InVoxelItem), true, true);
	return voxel;
}

bool UVoxelModuleBPLibrary::IsValid(UVoxel& InVoxel)
{
	return InVoxel.IsValidLowLevel() && !InVoxel.IsEmpty() && !InVoxel.IsUnknown();
}