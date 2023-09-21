// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/VoxelModuleBPLibrary.h"

#include "Asset/AssetModuleBPLibrary.h"
#include "Common/CommonBPLibrary.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Datas/VoxelData.h"
#include "Voxel/Voxels/Voxel.h"

FPrimaryAssetId UVoxelModuleBPLibrary::VoxelTypeToAssetID(EVoxelType InVoxelType)
{
	if(InVoxelType == EVoxelType::Empty) FPrimaryAssetId();

	return FPrimaryAssetId(FName("Voxel"), *FString::Printf(TEXT("DA_Voxel_%s"), *UCommonBPLibrary::GetEnumValueAuthoredName(TEXT("/Script/WHFramework.EVoxelType"), (int32)InVoxelType)));
}

FVoxelWorldSaveData& UVoxelModuleBPLibrary::GetWorldData()
{
	if(AVoxelModule* VoxelModule = AVoxelModule::Get())
	{
		return VoxelModule->GetWorldData();
	}
	return FVoxelWorldSaveData::Empty;
}

EVoxelWorldMode UVoxelModuleBPLibrary::GetWorldMode()
{
	if(AVoxelModule* VoxelModule = AVoxelModule::Get())
	{
		return VoxelModule->GetWorldMode();
	}
	return EVoxelWorldMode::None;
}

EVoxelWorldState UVoxelModuleBPLibrary::GetWorldState()
{
	if(AVoxelModule* VoxelModule = AVoxelModule::Get())
	{
		return VoxelModule->GetWorldState();
	}
	return EVoxelWorldState::None;
}

FVoxelWorldBasicSaveData UVoxelModuleBPLibrary::GetWorldBasicData()
{
	if(AVoxelModule* VoxelModule = AVoxelModule::Get())
	{
		return VoxelModule->GetWorldBasicData();
	}
	return FVoxelWorldBasicSaveData();
}

FIndex UVoxelModuleBPLibrary::LocationToChunkIndex(FVector InLocation, bool bIgnoreZ /*= false*/)
{
	if(AVoxelModule* VoxelModule = AVoxelModule::Get())
	{
		return VoxelModule->LocationToChunkIndex(InLocation, bIgnoreZ);
	}
	return FIndex::ZeroIndex;
}

FVector UVoxelModuleBPLibrary::ChunkIndexToLocation(FIndex InIndex)
{
	if(AVoxelModule* VoxelModule = AVoxelModule::Get())
	{
		return VoxelModule->ChunkIndexToLocation(InIndex);
	}
	return FVector::ZeroVector;
}

FIndex UVoxelModuleBPLibrary::LocationToVoxelIndex(FVector InLocation, bool bIgnoreZ)
{
	if(AVoxelModule* VoxelModule = AVoxelModule::Get())
	{
		return VoxelModule->LocationToVoxelIndex(InLocation, bIgnoreZ);
	}
	return FIndex::ZeroIndex;
}

FVector UVoxelModuleBPLibrary::VoxelIndexToLocation(FIndex InIndex)
{
	if(AVoxelModule* VoxelModule = AVoxelModule::Get())
	{
		return VoxelModule->VoxelIndexToLocation(InIndex);
	}
	return FVector::ZeroVector;
}

int32 UVoxelModuleBPLibrary::VoxelIndexToNumber(FIndex InIndex)
{
	if(AVoxelModule* VoxelModule = AVoxelModule::Get())
	{
		return VoxelModule->VoxelIndexToNumber(InIndex);
	}
	return 0;
}

FIndex UVoxelModuleBPLibrary::NumberToVoxelIndex(int32 InNumber)
{
	if(AVoxelModule* VoxelModule = AVoxelModule::Get())
	{
		return VoxelModule->NumberToVoxelIndex(InNumber);
	}
	return FIndex::ZeroIndex;
}

AVoxelChunk* UVoxelModuleBPLibrary::FindChunkByIndex(FIndex InIndex)
{
	if(AVoxelModule* VoxelModule = AVoxelModule::Get())
	{
		return VoxelModule->FindChunkByIndex(InIndex);
	}
	return nullptr;
}

AVoxelChunk* UVoxelModuleBPLibrary::FindChunkByLocation(FVector InLocation)
{
	if(AVoxelModule* VoxelModule = AVoxelModule::Get())
	{
		return VoxelModule->FindChunkByLocation(InLocation);
	}
	return nullptr;
}

FVoxelItem& UVoxelModuleBPLibrary::FindVoxelByIndex(FIndex InIndex)
{
	if(AVoxelModule* VoxelModule = AVoxelModule::Get())
	{
		return VoxelModule->FindVoxelByIndex(InIndex);
	}
	return FVoxelItem::Empty;
}

FVoxelItem& UVoxelModuleBPLibrary::FindVoxelByLocation(FVector InLocation)
{
	if(AVoxelModule* VoxelModule = AVoxelModule::Get())
	{
		return VoxelModule->FindVoxelByLocation(InLocation);
	}
	return FVoxelItem::Empty;
}

UVoxel& UVoxelModuleBPLibrary::GetVoxel(EVoxelType InVoxelType)
{
	return GetVoxel(VoxelTypeToAssetID(InVoxelType));
}

UVoxel& UVoxelModuleBPLibrary::GetVoxel(const FPrimaryAssetId& InVoxelID)
{
	const UVoxelData& voxelData = UAssetModuleBPLibrary::LoadPrimaryAssetRef<UVoxelData>(InVoxelID);
	if(voxelData.IsValid())
	{
		const TSubclassOf<UVoxel> tmpClass = voxelData.VoxelClass ? voxelData.VoxelClass : UVoxel::StaticClass();
		UVoxel& voxel = UReferencePoolModuleBPLibrary::GetReference<UVoxel>(true, tmpClass);
		voxel.SetItem(InVoxelID);
		return voxel;
	}
	return UReferencePoolModuleBPLibrary::GetReference<UVoxel>(true);
}

UVoxel& UVoxelModuleBPLibrary::GetVoxel(const FVoxelItem& InVoxelItem)
{
	if(InVoxelItem.IsEmpty()) return UVoxel::GetEmpty();
	if(InVoxelItem.IsUnknown()) return UVoxel::GetUnknown();
	UVoxel& voxel = GetVoxel(InVoxelItem.ID);
	voxel.SetItem(InVoxelItem);
	if(!InVoxelItem.Data.IsEmpty())
	{
		voxel.LoadData(InVoxelItem.Data);
	}
	return voxel;
}

EVoxelType UVoxelModuleBPLibrary::GetNoiseVoxelType(FIndex InIndex)
{
	if(AVoxelModule* VoxelModule = AVoxelModule::Get())
	{
		return VoxelModule->GetNoiseVoxelType(InIndex);
	}
	return EVoxelType::Empty;
}

ECollisionChannel UVoxelModuleBPLibrary::GetChunkTraceChannel()
{
	if(AVoxelModule* VoxelModule = AVoxelModule::Get())
	{
		return VoxelModule->GetChunkTraceChannel();
	}
	return ECollisionChannel::ECC_MAX;
}

ECollisionChannel UVoxelModuleBPLibrary::GetVoxelTraceChannel()
{
	if(AVoxelModule* VoxelModule = AVoxelModule::Get())
	{
		return VoxelModule->GetVoxelTraceChannel();
	}
	return ECollisionChannel::ECC_MAX;
}

bool UVoxelModuleBPLibrary::VoxelRaycastSinge(FVector InRayStart, FVector InRayEnd, const TArray<AActor*>& InIgnoreActors, FVoxelHitResult& OutHitResult)
{
	if(AVoxelModule* VoxelModule = AVoxelModule::Get())
	{
		return VoxelModule->VoxelRaycastSinge(InRayStart, InRayEnd, InIgnoreActors, OutHitResult);
	}
	return false;
}

bool UVoxelModuleBPLibrary::VoxelRaycastSinge(EVoxelRaycastType InRaycastType, float InDistance, const TArray<AActor*>& InIgnoreActors, FVoxelHitResult& OutHitResult)
{
	if(AVoxelModule* VoxelModule = AVoxelModule::Get())
	{
		return VoxelModule->VoxelRaycastSinge(InRaycastType, InDistance, InIgnoreActors, OutHitResult);
	}
	return false;
}

bool UVoxelModuleBPLibrary::VoxelItemTraceSingle(const FVoxelItem& InVoxelItem, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult)
{
	if(AVoxelModule* VoxelModule = AVoxelModule::Get())
	{
		return VoxelModule->VoxelItemTraceSingle(InVoxelItem, InIgnoreActors, OutHitResult);
	}
	return false;
}

bool UVoxelModuleBPLibrary::VoxelAgentTraceSingle(FIndex InChunkIndex, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult, bool bSnapToBlock, int32 InMaxCount, bool bFromCenter)
{
	if(AVoxelModule* VoxelModule = AVoxelModule::Get())
	{
		return VoxelModule->VoxelAgentTraceSingle(InChunkIndex, InRadius, InHalfHeight, InIgnoreActors, OutHitResult, bSnapToBlock, InMaxCount, bFromCenter);
	}
	return false;
}

bool UVoxelModuleBPLibrary::VoxelAgentTraceSingle(FVector InLocation, FVector2D InRange, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult, bool bSnapToBlock, int32 InMaxCount, bool bFromCenter)
{
	if(AVoxelModule* VoxelModule = AVoxelModule::Get())
	{
		return VoxelModule->VoxelAgentTraceSingle(InLocation, InRange, InRadius, InHalfHeight, InIgnoreActors, OutHitResult, bSnapToBlock, InMaxCount, bFromCenter);
	}
	return false;
}

bool UVoxelModuleBPLibrary::VoxelAgentTraceSingle(FVector InRayStart, FVector InRayEnd, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult)
{
	if(AVoxelModule* VoxelModule = AVoxelModule::Get())
	{
		return VoxelModule->VoxelAgentTraceSingle(InRayStart, InRayEnd, InRadius, InHalfHeight, InIgnoreActors, OutHitResult);
	}
	return false;
}
