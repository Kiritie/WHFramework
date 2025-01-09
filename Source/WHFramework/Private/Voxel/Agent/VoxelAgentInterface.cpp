#include "Voxel/Agent/VoxelAgentInterface.h"

#include "Common/CommonStatics.h"
#include "Event/EventModuleStatics.h"
#include "Event/Handle/Voxel/EventHandle_VoxelDestroyed.h"
#include "Event/Handle/Voxel/EventHandle_VoxelGenerated.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Datas/VoxelData.h"
#include "Voxel/Voxels/Voxel.h"
#include "Voxel/Voxels/Auxiliary/VoxelAuxiliary.h"

IVoxelAgentInterface::IVoxelAgentInterface()
{
	InteractVoxelItem = FVoxelItem::Empty;
	InteractVoxelRemaining = 0.f;
}

bool IVoxelAgentInterface::OnGenerateVoxel(EInputInteractEvent InInteractEvent, const FVoxelHitResult& InHitResult)
{
	FPrimaryAssetId GenerateVoxelID = GetGenerateVoxelID();
	if(!GenerateVoxelID.IsValid()) return false;
	
	AVoxelChunk* Chunk = InHitResult.GetChunk();
	if(!Chunk) return false;

	FVoxelItem VoxelItem = GenerateVoxelID;
	VoxelItem.Owner = Chunk;
	VoxelItem.Index = Chunk->LocationToIndex(InHitResult.Point - UVoxelModule::Get().GetWorldData().GetBlockSizedNormal(InHitResult.Normal)) + FIndex(InHitResult.Normal);
	const float Angle = (GetVoxelAgentLocation() - (VoxelItem.GetLocation() + UVoxelModule::Get().GetWorldData().BlockSize * 0.5f)).GetSafeNormal2D().ToOrientationRotator().Yaw;
	VoxelItem.Angle = (ERightAngle)FMath::RoundToInt((Angle >= 0.f ? Angle : (360.f + Angle)) / 90.f);

	TArray<AActor*> IgnoreActors;
	if(VoxelItem.Auxiliary)
	{
		IgnoreActors.Add(VoxelItem.Auxiliary);
	}

	switch(InInteractEvent)
	{
		case EInputInteractEvent::Started:
		{
			break;
		}
		case EInputInteractEvent::Triggered:
		{
			break;
		}
		case EInputInteractEvent::Completed:
		{
			FHitResult HitResult;
			if(!UVoxelModuleStatics::VoxelItemTraceSingle(VoxelItem, IgnoreActors, HitResult))
			{
				if(Chunk->SetVoxelComplex(VoxelItem.Index, VoxelItem, true, this))
				{
					UEventModuleStatics::BroadcastEvent<UEventHandle_VoxelGenerated>(Cast<UObject>(this), { &VoxelItem, Cast<UObject>(this) });
					return true;
				}
			}
			break;
		}
	}
	return false;
}

bool IVoxelAgentInterface::OnDestroyVoxel(EInputInteractEvent InInteractEvent, const FVoxelHitResult& InHitResult)
{
	AVoxelChunk* Chunk = InHitResult.GetChunk();
	if(!Chunk) return false;

	switch(InInteractEvent)
	{
		case EInputInteractEvent::Started:
		{
			InteractVoxelItem = FVoxelItem::Empty;
			return true;
		}
		case EInputInteractEvent::Triggered:
		{
			if(InteractVoxelItem == FVoxelItem::Unknown) break;
			
			if(!InteractVoxelItem.EqualIndex(InHitResult.VoxelItem))
			{
				InteractVoxelItem = InHitResult.VoxelItem;
				InteractVoxelRemaining = InteractVoxelItem.GetVoxelData().Hardness;
			}

			InteractVoxelRemaining -= UCommonStatics::GetCurrentDeltaSeconds() * GetDestroyVoxelRate();
			if(InteractVoxelRemaining <= 0.f)
			{
				if(Chunk->SetVoxelComplex(InteractVoxelItem.Index, FVoxelItem::Empty, true, this))
				{
					UEventModuleStatics::BroadcastEvent<UEventHandle_VoxelDestroyed>(Cast<UObject>(this), { &InteractVoxelItem, Cast<UObject>(this) });
					InteractVoxelItem = FVoxelItem::Empty;
					return true;
				}
			}
			break;
		}
		case EInputInteractEvent::Completed:
		{
			return true;
		}
	}
	return false;
}

bool IVoxelAgentInterface::OnInteractVoxel(EInputInteractAction InInteractAction, EInputInteractEvent InInteractEvent, const FVoxelHitResult& InHitResult)
{
	return InHitResult.GetVoxel().OnAgentInteract(this, InInteractAction, InInteractEvent, InHitResult);
}

float IVoxelAgentInterface::GetInteractVoxelProgress() const
{
	if(InteractVoxelItem.IsValid())
	{
		const float VoxelHardness = InteractVoxelItem.GetVoxelData().Hardness;
		return (VoxelHardness - InteractVoxelRemaining) / VoxelHardness;
	}
	return 0.f;
}
