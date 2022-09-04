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
	return AVoxelModule::GetWorldData();
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
	FIndex chunkIndex = FIndex(FMath::FloorToInt(InLocation.X / GetWorldData().GetChunkLength()),
		FMath::FloorToInt(InLocation.Y / GetWorldData().GetChunkLength()),
		bIgnoreZ ? 0 : FMath::FloorToInt(InLocation.Z / GetWorldData().GetChunkLength()));
	return chunkIndex;
}

FVector UVoxelModuleBPLibrary::ChunkIndexToLocation(FIndex InIndex)
{
	return InIndex.ToVector() * GetWorldData().GetChunkLength();
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
	const int32 worldHeight = GetWorldData().GetWorldHeight();

	const int32 plainHeight = UMathBPLibrary::GetNoiseHeight(InIndex, GetWorldData().TerrainPlainScale, GetWorldData().WorldSeed) * worldHeight;
	const int32 mountainHeight = UMathBPLibrary::GetNoiseHeight(InIndex, GetWorldData().TerrainMountainScale, GetWorldData().WorldSeed) * worldHeight;

	const int32 baseHeight = FMath::Max(plainHeight, mountainHeight) + worldHeight * GetWorldData().TerrainBaseHeight;

	const int32 stoneHeight = UMathBPLibrary::GetNoiseHeight(InIndex, GetWorldData().TerrainStoneVoxelScale, GetWorldData().WorldSeed) * worldHeight;
	const int32 sandHeight = UMathBPLibrary::GetNoiseHeight(InIndex, GetWorldData().TerrainSandVoxelScale, GetWorldData().WorldSeed) * worldHeight;

	const int32 waterHeight = worldHeight * GetWorldData().TerrainWaterVoxelHeight;
	const int32 bedrockHeight = worldHeight * GetWorldData().TerrainBedrockVoxelHeight;

	if(InIndex.Z < baseHeight)
	{
		if(InIndex.Z <= bedrockHeight)
		{
			return EVoxelType::Bedrock; //Bedrock
		}
		else if(InIndex.Z <= stoneHeight)
		{
			return EVoxelType::Stone; //Stone
		}
		return EVoxelType::Dirt; //Dirt
	}
	else if(InIndex.Z <= sandHeight)
	{
		return EVoxelType::Sand; //Sand
	}
	else if(InIndex.Z <= waterHeight)
	{
		return EVoxelType::Water; //Water
	}
	else if(InIndex.Z == baseHeight)
	{
		return EVoxelType::Grass; //Grass
	}
	return EVoxelType::Empty; //Empty
}

bool UVoxelModuleBPLibrary::ChunkTraceSingle(FIndex InChunkIndex, float InRadius, float InHalfHeight, ECollisionChannel InChunkTraceType, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult)
{
	FVector chunkPos = UVoxelModuleBPLibrary::ChunkIndexToLocation(InChunkIndex);
	FVector rayStart = FVector(GetWorldData().RandomStream.FRandRange(1.f, GetWorldData().ChunkSize - 1), GetWorldData().RandomStream.FRandRange(1.f, GetWorldData().ChunkSize - 1), GetWorldData().ChunkSize * GetWorldData().ChunkHeightRange) * GetWorldData().BlockSize;
	rayStart.X = chunkPos.X + ((int32)(rayStart.X / GetWorldData().BlockSize) + 0.5f) * GetWorldData().BlockSize;
	rayStart.Y = chunkPos.Y + ((int32)(rayStart.Y / GetWorldData().BlockSize) + 0.5f) * GetWorldData().BlockSize;
	FVector rayEnd = FVector(rayStart.X, rayStart.Y, 0);
	return ChunkTraceSingle(rayStart, rayEnd, InRadius, InHalfHeight, InChunkTraceType, InIgnoreActors, OutHitResult);
}

bool UVoxelModuleBPLibrary::ChunkTraceSingle(FVector InRayStart, FVector InRayEnd, float InRadius, float InHalfHeight, ECollisionChannel InChunkTraceType, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult)
{
	return UKismetSystemLibrary::CapsuleTraceSingle(AMainModule::Get(), InRayStart, InRayEnd, InRadius, InHalfHeight, UGlobalBPLibrary::GetGameTraceChannel(InChunkTraceType), false, InIgnoreActors, EDrawDebugTrace::None, OutHitResult, true);
}

bool UVoxelModuleBPLibrary::VoxelTraceSingle(const FVoxelItem& InVoxelItem, ECollisionChannel InVoxelTraceType, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult)
{
	const FVector size = InVoxelItem.GetRange() * GetWorldData().BlockSize * 0.5f;
	const FVector location = InVoxelItem.GetLocation();
	return UKismetSystemLibrary::BoxTraceSingle(AMainModule::Get(), location + size, location + size, size, FRotator::ZeroRotator, UGlobalBPLibrary::GetGameTraceChannel(InVoxelTraceType), false, InIgnoreActors, EDrawDebugTrace::None, OutHitResult, true);
}

bool UVoxelModuleBPLibrary::VoxelRaycastSinge(FVector InRayStart, FVector InRayEnd, ECollisionChannel InVoxelTraceType, const TArray<AActor*>& InIgnoreActors, FVoxelHitResult& OutHitResult)
{
	FHitResult hitResult;
	if(UKismetSystemLibrary::LineTraceSingle(AMainModule::Get(), InRayStart, InRayEnd, UGlobalBPLibrary::GetGameTraceChannel(InVoxelTraceType), false, InIgnoreActors, EDrawDebugTrace::None, hitResult, true))
	{
		OutHitResult = FVoxelHitResult(hitResult);
		return OutHitResult.IsValid();
	}
	return false;
}

bool UVoxelModuleBPLibrary::VoxelRaycastSinge(float InDistance, ECollisionChannel InVoxelTraceType, const TArray<AActor*>& InIgnoreActors, FVoxelHitResult& OutHitResult)
{
	FHitResult hitResult;
	const AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController();
	if(PlayerController && PlayerController->RaycastSingleFromAimPoint(InDistance, InVoxelTraceType, InIgnoreActors, hitResult))
	{
		OutHitResult = FVoxelHitResult(hitResult);
		return OutHitResult.IsValid();
	}
	return false;
}

ECollisionChannel UVoxelModuleBPLibrary::GetChunkTraceType()
{
	if(AVoxelModule* VoxelModule = AMainModule::GetModuleByClass<AVoxelModule>())
	{
		return VoxelModule->GetChunkTraceType();
	}
	return ECollisionChannel::ECC_MAX;
}

ECollisionChannel UVoxelModuleBPLibrary::GetVoxelTraceType()
{
	if(AVoxelModule* VoxelModule = AMainModule::GetModuleByClass<AVoxelModule>())
	{
		return VoxelModule->GetVoxelTraceType();
	}
	return ECollisionChannel::ECC_MAX;
}
