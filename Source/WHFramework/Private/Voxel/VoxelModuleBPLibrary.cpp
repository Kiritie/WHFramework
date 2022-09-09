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
	if(InVoxelType == EVoxelType::Empty) FPrimaryAssetId();

	return FPrimaryAssetId(FName("Voxel"), *FString::Printf(TEXT("DA_Voxel_%s"), *UGlobalBPLibrary::GetEnumValueAuthoredName(TEXT("EVoxelType"), (int32)InVoxelType)));
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

float UVoxelModuleBPLibrary::GetWorldLength()
{
	if(AVoxelModule* VoxelModule = AVoxelModule::Get())
	{
		return VoxelModule->GetWorldLength();
	}
	return 0.f;
}

bool UVoxelModuleBPLibrary::IsBasicGenerated()
{
	if(AVoxelModule* VoxelModule = AVoxelModule::Get())
	{
		return VoxelModule->IsBasicGenerated();
	}
	return false;
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
	if(AVoxelModule* VoxelModule = AVoxelModule::Get())
	{
		return VoxelModule->GetNoiseVoxelType(InIndex);
	}
	return EVoxelType::Empty;
}

bool UVoxelModuleBPLibrary::ChunkTraceSingle(FIndex InChunkIndex, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult)
{
	const auto& worldData = GetWorldData();
	const float& chunkLength = worldData.GetChunkLength();
	const FVector chunkLocation = ChunkIndexToLocation(InChunkIndex);
	FVector rayStart = FVector(worldData.RandomStream.FRandRange(0.f, chunkLength), worldData.RandomStream.FRandRange(0.f, chunkLength), worldData.GetWorldHeight(true));
	rayStart.X = chunkLocation.X + ((int32)(rayStart.X / worldData.BlockSize) + 0.5f) * worldData.BlockSize;
	rayStart.Y = chunkLocation.Y + ((int32)(rayStart.Y / worldData.BlockSize) + 0.5f) * worldData.BlockSize;
	const FVector rayEnd = FVector(rayStart.X, rayStart.Y, 0.f);
	return ChunkTraceSingle(rayStart, rayEnd, InRadius, InHalfHeight, InIgnoreActors, OutHitResult);
}

bool UVoxelModuleBPLibrary::ChunkTraceSingle(FVector InRayStart, FVector InRayEnd, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult)
{
	return UKismetSystemLibrary::CapsuleTraceSingle(AMainModule::Get(), InRayStart, InRayEnd, InRadius, InHalfHeight, UGlobalBPLibrary::GetGameTraceChannel(GetChunkTraceType()), false, InIgnoreActors, EDrawDebugTrace::None, OutHitResult, true);
}

bool UVoxelModuleBPLibrary::VoxelTraceSingle(const FVoxelItem& InVoxelItem, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult)
{
	const FVector size = InVoxelItem.GetRange() * GetWorldData().BlockSize * 0.5f;
	const FVector location = InVoxelItem.GetLocation();
	return UKismetSystemLibrary::BoxTraceSingle(AMainModule::Get(), location + size, location + size, size * 0.95f, FRotator::ZeroRotator, UGlobalBPLibrary::GetGameTraceChannel(GetVoxelTraceType()), false, InIgnoreActors, EDrawDebugTrace::None, OutHitResult, true);
}

bool UVoxelModuleBPLibrary::VoxelTraceSingle(FVector InRayStart, FVector InRayEnd, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult)
{
	return UKismetSystemLibrary::CapsuleTraceSingle(AMainModule::Get(), InRayStart, InRayEnd, InRadius, InHalfHeight, UGlobalBPLibrary::GetGameTraceChannel(GetVoxelTraceType()), false, InIgnoreActors, EDrawDebugTrace::None, OutHitResult, true);
}

bool UVoxelModuleBPLibrary::VoxelRaycastSinge(FVector InRayStart, FVector InRayEnd, const TArray<AActor*>& InIgnoreActors, FVoxelHitResult& OutHitResult)
{
	FHitResult hitResult;
	if(UKismetSystemLibrary::LineTraceSingle(AMainModule::Get(), InRayStart, InRayEnd, UGlobalBPLibrary::GetGameTraceChannel(GetVoxelTraceType()), false, InIgnoreActors, EDrawDebugTrace::None, hitResult, true))
	{
		OutHitResult = FVoxelHitResult(hitResult);
		return OutHitResult.IsValid();
	}
	return false;
}

bool UVoxelModuleBPLibrary::VoxelRaycastSinge(float InDistance, const TArray<AActor*>& InIgnoreActors, FVoxelHitResult& OutHitResult)
{
	FHitResult hitResult;
	const AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController();
	if(PlayerController && PlayerController->RaycastSingleFromAimPoint(InDistance, GetVoxelTraceType(), InIgnoreActors, hitResult))
	{
		OutHitResult = FVoxelHitResult(hitResult);
		return OutHitResult.IsValid();
	}
	return false;
}

ECollisionChannel UVoxelModuleBPLibrary::GetChunkTraceType()
{
	if(AVoxelModule* VoxelModule = AVoxelModule::Get())
	{
		return VoxelModule->GetChunkTraceType();
	}
	return ECollisionChannel::ECC_MAX;
}

ECollisionChannel UVoxelModuleBPLibrary::GetVoxelTraceType()
{
	if(AVoxelModule* VoxelModule = AVoxelModule::Get())
	{
		return VoxelModule->GetVoxelTraceType();
	}
	return ECollisionChannel::ECC_MAX;
}
