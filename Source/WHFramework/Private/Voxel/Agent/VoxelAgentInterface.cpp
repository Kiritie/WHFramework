#pragma once

#include "Voxel/Agent/VoxelAgentInterface.h"

#include "Event/EventModuleBPLibrary.h"
#include "Event/Handle/Voxel/EventHandle_DestroyVoxel.h"
#include "Event/Handle/Voxel/EventHandle_GenerateVoxel.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleBPLibrary.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Datas/VoxelData.h"
#include "Voxel/Voxels/Voxel.h"
#include "Voxel/Voxels/Auxiliary/VoxelAuxiliary.h"

bool IVoxelAgentInterface::OnGenerateVoxel(const FVoxelHitResult& InVoxelHitResult)
{
	if(!GetGenerateVoxelID().IsValid()) return false;
	
	AVoxelChunk* Chunk = InVoxelHitResult.GetChunk();

	if(!Chunk) return false;

	FVoxelItem VoxelItem = GetGenerateVoxelID();
	VoxelItem.Owner = Chunk;
	VoxelItem.Index = Chunk->LocationToIndex(InVoxelHitResult.Point - AVoxelModule::Get()->GetWorldData().GetBlockSizedNormal(InVoxelHitResult.Normal)) + FIndex(InVoxelHitResult.Normal);
	const float Angle = (GetAgentLocation() - (VoxelItem.GetLocation() + AVoxelModule::Get()->GetWorldData().BlockSize * 0.5f)).GetSafeNormal2D().ToOrientationRotator().Yaw;
	VoxelItem.Angle = (ERightAngle)FMath::RoundToInt((Angle >= 0.f ? Angle : (360.f + Angle)) / 90.f);
	
	TArray<AActor*> IgnoreActors;
	if(VoxelItem.Auxiliary)
	{
		IgnoreActors.Add(VoxelItem.Auxiliary);
	}
	FHitResult HitResult;
	if(!UVoxelModuleBPLibrary::VoxelItemTraceSingle(VoxelItem, IgnoreActors, HitResult))
	{
		if(Chunk->SetVoxelComplex(VoxelItem.Index, VoxelItem, true, this))
		{
			UEventModuleBPLibrary::BroadcastEvent<UEventHandle_GenerateVoxel>(EEventNetType::Single, Cast<UObject>(this), { &VoxelItem });
			return true;
		}
	}
	return false;
}

bool IVoxelAgentInterface::OnDestroyVoxel(const FVoxelHitResult& InVoxelHitResult)
{
	AVoxelChunk* Chunk = InVoxelHitResult.GetChunk();

	if(!Chunk) return false;
	
	FVoxelItem VoxelItem = InVoxelHitResult.VoxelItem;
	if(Chunk->SetVoxelComplex(VoxelItem.Index, FVoxelItem::Empty, true, this))
	{
		UEventModuleBPLibrary::BroadcastEvent<UEventHandle_DestroyVoxel>(EEventNetType::Single, Cast<UObject>(this), { &VoxelItem });
		return true;
	}
	return false;
}

bool IVoxelAgentInterface::OnInteractVoxel(const FVoxelHitResult& InVoxelHitResult, EInputInteractAction InInteractAction)
{
	return InVoxelHitResult.GetVoxel().OnAgentInteract(this, InInteractAction, InVoxelHitResult);
}
