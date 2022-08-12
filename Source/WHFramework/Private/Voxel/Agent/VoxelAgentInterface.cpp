#pragma once

#include "Voxel/Agent/VoxelAgentInterface.h"

#include "Voxel/VoxelModuleBPLibrary.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Datas/VoxelData.h"

bool IVoxelAgentInterface::GenerateVoxel(const FVoxelHitResult& InVoxelHitResult)
{
	if(!GetGenerateVoxelItem().IsValid()) return false;
	
	if(AVoxelChunk* Chunk = InVoxelHitResult.GetChunk())
	{
		FVoxelItem VoxelItem = GetGenerateVoxelItem();
		VoxelItem.Owner = Chunk;
		VoxelItem.Index = Chunk->LocationToIndex(InVoxelHitResult.Point - UVoxelModuleBPLibrary::GetWorldData().GetBlockSizedNormal(InVoxelHitResult.Normal)) + FIndex(InVoxelHitResult.Normal);

		// FRotator Rotation = (Chunk->IndexToLocation(VoxelItem.Index) + VoxelItem.GetVoxelData().GetRange() * 0.5f * UVoxelModuleBPLibrary::GetWorldData().BlockSize - GetWorldLocation()).ToOrientationRotator();
		// Rotation = FRotator(FRotator::ClampAxis(FMath::RoundToInt(Rotation.Pitch / 90) * 90.f), FRotator::ClampAxis(FMath::RoundToInt(Rotation.Yaw / 90) * 90.f), FRotator::ClampAxis(FMath::RoundToInt(Rotation.Roll / 90) * 90.f));
		// VoxelItem.Rotation = Rotation;
		
		FHitResult HitResult;
		if (!UVoxelModuleBPLibrary::VoxelTraceSingle(VoxelItem, Chunk->IndexToLocation(VoxelItem.Index), HitResult))
		{
			return Chunk->SetVoxelComplex(VoxelItem.Index, VoxelItem, true, this);
		}
	}
	return false;
}

bool IVoxelAgentInterface::DestroyVoxel(const FVoxelHitResult& InVoxelHitResult)
{
	AVoxelChunk* Chunk = InVoxelHitResult.GetChunk();
	const FVoxelItem& VoxelItem = InVoxelHitResult.VoxelItem;
	if (VoxelItem.GetVoxelType() != EVoxelType::Bedrock)
	{
		return Chunk->SetVoxelComplex(VoxelItem.Index, FVoxelItem::Empty, true, this);
	}
	return false;
}
