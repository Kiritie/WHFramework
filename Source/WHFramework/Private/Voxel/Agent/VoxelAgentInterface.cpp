#pragma once

#include "Voxel/Agent/VoxelAgentInterface.h"

#include "Voxel/VoxelModuleBPLibrary.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Datas/VoxelData.h"

bool IVoxelAgentInterface::GenerateVoxel(const FVoxelHitResult& InVoxelHitResult)
{
	if(!GetGenerateVoxelID().IsValid()) return false;
	
	AVoxelChunk* Chunk = InVoxelHitResult.GetChunk();
	
	FVoxelItem VoxelItem;
	VoxelItem.ID = GetGenerateVoxelID();
	VoxelItem.Owner = Chunk;
	VoxelItem.Index = Chunk->LocationToIndex(InVoxelHitResult.Point - UVoxelModuleBPLibrary::GetWorldData().GetBlockSizedNormal(InVoxelHitResult.Normal)) + FIndex(InVoxelHitResult.Normal);
	const FRotator Rotation = (VoxelItem.GetLocation() + UVoxelModuleBPLibrary::GetWorldData().BlockSize * 0.5f - GetWorldLocation()).ToOrientationRotator();
	VoxelItem.Angle = (ERightAngle)(FMath::RoundToInt((Rotation.Yaw >= 0.f ? Rotation.Yaw : (360.f + Rotation.Yaw)) / 90.f));
	
	FHitResult HitResult;
	if(!UVoxelModuleBPLibrary::VoxelTraceSingle(VoxelItem, VoxelItem.GetLocation(), HitResult))
	{
		return Chunk->SetVoxelComplex(VoxelItem.Index, VoxelItem, true, this);
	}
	 return false;
}

bool IVoxelAgentInterface::DestroyVoxel(const FVoxelHitResult& InVoxelHitResult)
{
	AVoxelChunk* Chunk = InVoxelHitResult.GetChunk();
	const FVoxelItem& VoxelItem = InVoxelHitResult.VoxelItem;
	return Chunk->SetVoxelComplex(VoxelItem.Index, FVoxelItem::Empty, true, this);
}
