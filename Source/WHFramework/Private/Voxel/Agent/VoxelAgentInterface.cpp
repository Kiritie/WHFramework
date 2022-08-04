#pragma once

#include "Voxel/Agent/VoxelAgentInterface.h"

#include "Voxel/VoxelModuleBPLibrary.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Datas/VoxelData.h"

bool IVoxelAgentInterface::GenerateVoxel(const FVoxelHitResult& InVoxelHitResult)
{
	if(!GetGenerateVoxelItem().IsValid()) return false;
	
	bool bSuccess = false;
	if(AVoxelChunk* Chunk = InVoxelHitResult.GetChunk())
	{
		FVoxelItem NewVoxelItem = GetGenerateVoxelItem();
		NewVoxelItem.Owner = Chunk;
		NewVoxelItem.Index = Chunk->LocationToIndex(InVoxelHitResult.Point - UVoxelModuleBPLibrary::GetWorldData().GetBlockSizedNormal(InVoxelHitResult.Normal)) + FIndex(InVoxelHitResult.Normal);
		FVoxelItem OldVoxelItem = Chunk->GetVoxelItem(NewVoxelItem.Index);

		if(!OldVoxelItem.IsValid() || OldVoxelItem.GetData<UVoxelData>().Transparency == EVoxelTransparency::Transparent && OldVoxelItem != NewVoxelItem)
		{
			// FRotator rotation = (Chunk->IndexToLocation(NewVoxelItem.Index) + NewVoxelItem.GetData<UVoxelData>().GetCeilRange() * 0.5f * UVoxelModuleBPLibrary::GetWorldData().BlockSize - GetActorLocation()).ToOrientationRotator();
			// rotation = FRotator(FRotator::ClampAxis(FMath::RoundToInt(rotation.Pitch / 90) * 90.f), FRotator::ClampAxis(FMath::RoundToInt(rotation.Yaw / 90) * 90.f), FRotator::ClampAxis(FMath::RoundToInt(rotation.Roll / 90) * 90.f));
			// NewVoxelItem.Rotation = rotation;
			FHitResult HitResult;
			if (!UVoxelModuleBPLibrary::VoxelTraceSingle(NewVoxelItem, Chunk->IndexToLocation(NewVoxelItem.Index), HitResult))
			{
				if(OldVoxelItem.IsValid())
				{
					bSuccess = Chunk->ReplaceVoxel(OldVoxelItem, NewVoxelItem, this);
				}
				else
				{
					bSuccess = Chunk->GenerateVoxel(NewVoxelItem.Index, NewVoxelItem, this);
				}
			}
		}
	}
	return bSuccess;
}

bool IVoxelAgentInterface::DestroyVoxel(const FVoxelHitResult& InVoxelHitResult)
{
	AVoxelChunk* Chunk = InVoxelHitResult.GetChunk();
	const FVoxelItem& VoxelItem = InVoxelHitResult.VoxelItem;
	if (VoxelItem.GetData<UVoxelData>().VoxelType != EVoxelType::Bedrock)
	{
		return Chunk->DestroyVoxel(VoxelItem);
	}
	return false;
}
