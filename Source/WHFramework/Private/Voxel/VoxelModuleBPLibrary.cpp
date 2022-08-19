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
	if(InVoxelType != EVoxelType::Empty)
	{
		return FPrimaryAssetId(FName("Voxel"), *FString::Printf(TEXT("DA_Voxel_%s"), *UGlobalBPLibrary::GetEnumValueAuthoredName(TEXT("EVoxelType"), (int32)InVoxelType)));
	}
	return FPrimaryAssetId();
}

EVoxelType UVoxelModuleBPLibrary::AssetIDToVoxelType(FPrimaryAssetId InAssetID)
{
	if(InAssetID.IsValid())
	{
		return (EVoxelType)UGlobalBPLibrary::GetEnumIndexByValueName(TEXT("EVoxelType"), InAssetID.PrimaryAssetName.ToString().Mid(9));
	}
	return EVoxelType::Empty;
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

FVoxelWorldBasicSaveData UVoxelModuleBPLibrary::GetWorldBasicData()
{
	if(AVoxelModule* VoxelModule = AMainModule::GetModuleByClass<AVoxelModule>())
	{
		return VoxelModule->GetWorldBasicData();
	}
	return FVoxelWorldBasicSaveData();
}

float UVoxelModuleBPLibrary::GetWorldLength()
{
	if(AVoxelModule* VoxelModule = AMainModule::GetModuleByClass<AVoxelModule>())
	{
		return VoxelModule->GetWorldLength();
	}
	return 0.f;
}

bool UVoxelModuleBPLibrary::IsBasicGenerated()
{
	if(AVoxelModule* VoxelModule = AMainModule::GetModuleByClass<AVoxelModule>())
	{
		return VoxelModule->IsBasicGenerated();
	}
	return false;
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

AVoxelChunk* UVoxelModuleBPLibrary::FindChunkByIndex(FIndex InIndex)
{
	if(AVoxelModule* VoxelModule = AMainModule::GetModuleByClass<AVoxelModule>())
	{
		return VoxelModule->FindChunkByIndex(InIndex);
	}
	return nullptr;
}

AVoxelChunk* UVoxelModuleBPLibrary::FindChunkByLocation(FVector InLocation)
{
	if(AVoxelModule* VoxelModule = AMainModule::GetModuleByClass<AVoxelModule>())
	{
		return VoxelModule->FindChunkByLocation(InLocation);
	}
	return nullptr;
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
		const TSubclassOf<UVoxel> tmpClass = voxelData.VoxelClass ? voxelData.VoxelClass : UVoxel::StaticClass();
		UVoxel& voxel = UReferencePoolModuleBPLibrary::GetReference<UVoxel>(true, tmpClass);
		voxel.SetID(InVoxelID);
		return voxel;
	}
	return UReferencePoolModuleBPLibrary::GetReference<UVoxel>(true);
}

UVoxel& UVoxelModuleBPLibrary::GetVoxel(const FVoxelItem& InVoxelItem)
{
	if(InVoxelItem.IsEmpty()) return UVoxel::GetEmpty();
	if(InVoxelItem.IsUnknown()) return UVoxel::GetUnknown();
	UVoxel& voxel = GetVoxel(InVoxelItem.ID);
	voxel.LoadSaveData(&const_cast<FVoxelItem&>(InVoxelItem), true, true);
	return voxel;
}

EVoxelType UVoxelModuleBPLibrary::GetNoiseVoxelType(FIndex InIndex)
{
	if(AVoxelModule* VoxelModule = AMainModule::GetModuleByClass<AVoxelModule>())
	{
		return VoxelModule->GetNoiseVoxelType(InIndex);
	}
	return EVoxelType::Empty;
}

UVoxelData& UVoxelModuleBPLibrary::GetNoiseVoxelData(FIndex InIndex)
{
	if(AVoxelModule* VoxelModule = AMainModule::GetModuleByClass<AVoxelModule>())
	{
		return VoxelModule->GetNoiseVoxelData(InIndex);
	}
	return UReferencePoolModuleBPLibrary::GetReference<UVoxelData>();
}

int32 UVoxelModuleBPLibrary::GetNoiseTerrainHeight(FVector InOffset, FVector InScale)
{
	if(AVoxelModule* VoxelModule = AMainModule::GetModuleByClass<AVoxelModule>())
	{
		return VoxelModule->GetNoiseTerrainHeight(InOffset, InScale);
	}
	return -1;
}

bool UVoxelModuleBPLibrary::ChunkTraceSingle(AVoxelChunk* InChunk, float InRadius, float InHalfHeight, FHitResult& OutHitResult)
{
	if(AVoxelModule* VoxelModule = AMainModule::GetModuleByClass<AVoxelModule>())
	{
		return VoxelModule->ChunkTraceSingle(InChunk, InRadius, InHalfHeight, OutHitResult);
	}
	return false;
}

bool UVoxelModuleBPLibrary::ChunkTraceSingle(FVector RayStart, FVector RayEnd, float InRadius, float InHalfHeight, FHitResult& OutHitResult)
{
	if(AVoxelModule* VoxelModule = AMainModule::GetModuleByClass<AVoxelModule>())
	{
		return VoxelModule->ChunkTraceSingle(RayStart, RayEnd, InRadius, InHalfHeight, OutHitResult);
	}
	return false;
}

bool UVoxelModuleBPLibrary::VoxelTraceSingle(const FVoxelItem& InVoxelItem, FVector InPoint, FHitResult& OutHitResult)
{
	if(AVoxelModule* VoxelModule = AMainModule::GetModuleByClass<AVoxelModule>())
	{
		return VoxelModule->VoxelTraceSingle(InVoxelItem, InPoint, OutHitResult);
	}
	return false;
}
