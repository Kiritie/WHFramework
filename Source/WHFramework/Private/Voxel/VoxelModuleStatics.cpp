// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/VoxelModuleStatics.h"

#include "Asset/AssetModuleStatics.h"
#include "Common/CommonStatics.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Datas/VoxelData.h"
#include "Voxel/Voxels/Voxel.h"

FPrimaryAssetId UVoxelModuleStatics::VoxelTypeToAssetID(EVoxelType InVoxelType)
{
	if(InVoxelType == EVoxelType::Empty) FPrimaryAssetId();

	return FPrimaryAssetId(FName("Voxel"), *FString::Printf(TEXT("DA_Voxel_%s"), *UCommonStatics::GetEnumValueAuthoredName(TEXT("/Script/WHFramework.EVoxelType"), (int32)InVoxelType)));
}

FVoxelWorldSaveData& UVoxelModuleStatics::GetWorldData()
{
	if(UVoxelModule* VoxelModule = UVoxelModule::Get())
	{
		return VoxelModule->GetWorldData();
	}
	return FVoxelWorldSaveData::Empty;
}

EVoxelWorldMode UVoxelModuleStatics::GetWorldMode()
{
	if(UVoxelModule* VoxelModule = UVoxelModule::Get())
	{
		return VoxelModule->GetWorldMode();
	}
	return EVoxelWorldMode::None;
}

EVoxelWorldState UVoxelModuleStatics::GetWorldState()
{
	if(UVoxelModule* VoxelModule = UVoxelModule::Get())
	{
		return VoxelModule->GetWorldState();
	}
	return EVoxelWorldState::None;
}

FVoxelWorldBasicSaveData UVoxelModuleStatics::GetWorldBasicData()
{
	if(UVoxelModule* VoxelModule = UVoxelModule::Get())
	{
		return VoxelModule->GetWorldBasicData();
	}
	return FVoxelWorldBasicSaveData();
}

FIndex UVoxelModuleStatics::LocationToChunkIndex(FVector InLocation, bool bIgnoreZ /*= false*/)
{
	if(UVoxelModule* VoxelModule = UVoxelModule::Get())
	{
		return VoxelModule->LocationToChunkIndex(InLocation, bIgnoreZ);
	}
	return FIndex::ZeroIndex;
}

FVector UVoxelModuleStatics::ChunkIndexToLocation(FIndex InIndex)
{
	if(UVoxelModule* VoxelModule = UVoxelModule::Get())
	{
		return VoxelModule->ChunkIndexToLocation(InIndex);
	}
	return FVector::ZeroVector;
}

FIndex UVoxelModuleStatics::LocationToVoxelIndex(FVector InLocation, bool bIgnoreZ)
{
	if(UVoxelModule* VoxelModule = UVoxelModule::Get())
	{
		return VoxelModule->LocationToVoxelIndex(InLocation, bIgnoreZ);
	}
	return FIndex::ZeroIndex;
}

FVector UVoxelModuleStatics::VoxelIndexToLocation(FIndex InIndex)
{
	if(UVoxelModule* VoxelModule = UVoxelModule::Get())
	{
		return VoxelModule->VoxelIndexToLocation(InIndex);
	}
	return FVector::ZeroVector;
}

int32 UVoxelModuleStatics::VoxelIndexToNumber(FIndex InIndex)
{
	if(UVoxelModule* VoxelModule = UVoxelModule::Get())
	{
		return VoxelModule->VoxelIndexToNumber(InIndex);
	}
	return 0;
}

FIndex UVoxelModuleStatics::NumberToVoxelIndex(int32 InNumber)
{
	if(UVoxelModule* VoxelModule = UVoxelModule::Get())
	{
		return VoxelModule->NumberToVoxelIndex(InNumber);
	}
	return FIndex::ZeroIndex;
}

AVoxelChunk* UVoxelModuleStatics::FindChunkByIndex(FIndex InIndex)
{
	if(UVoxelModule* VoxelModule = UVoxelModule::Get())
	{
		return VoxelModule->FindChunkByIndex(InIndex);
	}
	return nullptr;
}

AVoxelChunk* UVoxelModuleStatics::FindChunkByLocation(FVector InLocation)
{
	if(UVoxelModule* VoxelModule = UVoxelModule::Get())
	{
		return VoxelModule->FindChunkByLocation(InLocation);
	}
	return nullptr;
}

FVoxelItem& UVoxelModuleStatics::FindVoxelByIndex(FIndex InIndex)
{
	if(UVoxelModule* VoxelModule = UVoxelModule::Get())
	{
		return VoxelModule->FindVoxelByIndex(InIndex);
	}
	return FVoxelItem::Empty;
}

FVoxelItem& UVoxelModuleStatics::FindVoxelByLocation(FVector InLocation)
{
	if(UVoxelModule* VoxelModule = UVoxelModule::Get())
	{
		return VoxelModule->FindVoxelByLocation(InLocation);
	}
	return FVoxelItem::Empty;
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
	if(UVoxelModule* VoxelModule = UVoxelModule::Get())
	{
		return VoxelModule->GetNoiseVoxelType(InIndex);
	}
	return EVoxelType::Empty;
}

EVoxelType UVoxelModuleStatics::GetRandomVoxelType(FIndex InIndex)
{
	if(UVoxelModule* VoxelModule = UVoxelModule::Get())
	{
		return VoxelModule->GetRandomVoxelType(InIndex);
	}
	return EVoxelType::Empty;
}

ECollisionChannel UVoxelModuleStatics::GetChunkTraceChannel()
{
	if(UVoxelModule* VoxelModule = UVoxelModule::Get())
	{
		return VoxelModule->GetChunkTraceChannel();
	}
	return ECC_MAX;
}

ECollisionChannel UVoxelModuleStatics::GetVoxelTraceChannel()
{
	if(UVoxelModule* VoxelModule = UVoxelModule::Get())
	{
		return VoxelModule->GetVoxelTraceChannel();
	}
	return ECC_MAX;
}

bool UVoxelModuleStatics::VoxelRaycastSinge(FVector InRayStart, FVector InRayEnd, const TArray<AActor*>& InIgnoreActors, FVoxelHitResult& OutHitResult)
{
	if(UVoxelModule* VoxelModule = UVoxelModule::Get())
	{
		return VoxelModule->VoxelRaycastSinge(InRayStart, InRayEnd, InIgnoreActors, OutHitResult);
	}
	return false;
}

bool UVoxelModuleStatics::VoxelRaycastSinge(EVoxelRaycastType InRaycastType, float InDistance, const TArray<AActor*>& InIgnoreActors, FVoxelHitResult& OutHitResult)
{
	if(UVoxelModule* VoxelModule = UVoxelModule::Get())
	{
		return VoxelModule->VoxelRaycastSinge(InRaycastType, InDistance, InIgnoreActors, OutHitResult);
	}
	return false;
}

bool UVoxelModuleStatics::VoxelItemTraceSingle(const FVoxelItem& InVoxelItem, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult)
{
	if(UVoxelModule* VoxelModule = UVoxelModule::Get())
	{
		return VoxelModule->VoxelItemTraceSingle(InVoxelItem, InIgnoreActors, OutHitResult);
	}
	return false;
}

bool UVoxelModuleStatics::VoxelAgentTraceSingle(FIndex InChunkIndex, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult, bool bSnapToBlock, int32 InMaxCount, bool bFromCenter)
{
	if(UVoxelModule* VoxelModule = UVoxelModule::Get())
	{
		return VoxelModule->VoxelAgentTraceSingle(InChunkIndex, InRadius, InHalfHeight, InIgnoreActors, OutHitResult, bSnapToBlock, InMaxCount, bFromCenter);
	}
	return false;
}

bool UVoxelModuleStatics::VoxelAgentTraceSingle(FVector InLocation, FVector2D InRange, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult, bool bSnapToBlock, int32 InMaxCount, bool bFromCenter)
{
	if(UVoxelModule* VoxelModule = UVoxelModule::Get())
	{
		return VoxelModule->VoxelAgentTraceSingle(InLocation, InRange, InRadius, InHalfHeight, InIgnoreActors, OutHitResult, bSnapToBlock, InMaxCount, bFromCenter);
	}
	return false;
}

bool UVoxelModuleStatics::VoxelAgentTraceSingle(FVector InRayStart, FVector InRayEnd, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult)
{
	if(UVoxelModule* VoxelModule = UVoxelModule::Get())
	{
		return VoxelModule->VoxelAgentTraceSingle(InRayStart, InRayEnd, InRadius, InHalfHeight, InIgnoreActors, OutHitResult);
	}
	return false;
}
