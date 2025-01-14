#include "Voxel/Agent/VoxelAgentInterface.h"

#include "Common/CommonStatics.h"
#include "Event/EventModuleStatics.h"
#include "Event/Handle/Voxel/EventHandle_VoxelDestroyed.h"
#include "Event/Handle/Voxel/EventHandle_VoxelGenerated.h"
#include "Math/MathStatics.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Datas/VoxelData.h"
#include "Voxel/Voxels/Voxel.h"
#include "Voxel/Voxels/Auxiliary/VoxelAuxiliary.h"
#include "Voxel/Voxels/Entity/VoxelEntityPreview.h"

IVoxelAgentInterface::IVoxelAgentInterface()
{
	bCanGenerateVoxel = false;
	GenerateVoxelItem = FVoxelItem::Empty;
	GeneratePreviewVoxel = nullptr;

	DestroyVoxelItem = FVoxelItem::Empty;
	DestroyVoxelRemaining = 0.f;
}

bool IVoxelAgentInterface::OnGenerateVoxel(EInputInteractEvent InInteractEvent, const FVoxelHitResult& InHitResult)
{
	AVoxelChunk* Chunk = InHitResult.GetChunk();
	if(!Chunk) return false;

	switch(InInteractEvent)
	{
		case EInputInteractEvent::Started:
		{
			GenerateVoxelItem = GetGenerateVoxelID();
			bCanGenerateVoxel = false;
			return true;
		}
		case EInputInteractEvent::Triggered:
		{
			if(!GenerateVoxelItem.IsValid()) break;

			GenerateVoxelItem.ID = GetGenerateVoxelID();

			if(!GenerateVoxelItem.IsValid()) break;

			GenerateVoxelItem.Owner = Chunk;
			GenerateVoxelItem.Index = Chunk->LocationToIndex(InHitResult.Point - UVoxelModule::Get().GetWorldData().GetBlockSizedNormal(InHitResult.Normal)) + FIndex(InHitResult.Normal);

			TArray<AActor*> IgnoreActors;
			if(GenerateVoxelItem.Auxiliary)
			{
				IgnoreActors.Add(GenerateVoxelItem.Auxiliary);
			}
			FHitResult HitResult;
			bCanGenerateVoxel = !UVoxelModuleStatics::VoxelItemTraceSingle(GenerateVoxelItem, IgnoreActors, HitResult);

			if(!GeneratePreviewVoxel)
			{
				GeneratePreviewVoxel = UObjectPoolModuleStatics::SpawnObject<AVoxelEntityPreview>();
				GeneratePreviewVoxel->LoadSaveData(&GenerateVoxelItem);
			}
			if(GeneratePreviewVoxel)
			{
				if(GenerateVoxelItem.GetVoxelData().bRotatable)
				{
					const ERightAngle Angle = UMathStatics::FloatToRightAngle((GetVoxelAgentLocation() - (GenerateVoxelItem.GetLocation() + UVoxelModule::Get().GetWorldData().BlockSize * 0.5f)).GetSafeNormal2D().ToOrientationRotator().Yaw);
					if(GenerateVoxelItem.Angle != Angle)
					{
						GenerateVoxelItem.Angle = Angle;
						GeneratePreviewVoxel->LoadSaveData(&GenerateVoxelItem);
					}
				}
				GeneratePreviewVoxel->SetActorLocation(GenerateVoxelItem.GetLocation());
				GeneratePreviewVoxel->SetMaterialColor(bCanGenerateVoxel ? FLinearColor(0.f, 0.7f, 0.f, 0.7f) : FLinearColor(1.f, 0.f, 0.f, 0.7f));
			}
			return true;
		}
		case EInputInteractEvent::Completed:
		{
			if(GeneratePreviewVoxel)
			{
				UObjectPoolModuleStatics::DespawnObject(GeneratePreviewVoxel);
				GeneratePreviewVoxel = nullptr;
			}

			if(!GenerateVoxelItem.IsValid() || !bCanGenerateVoxel) break;

			if(Chunk->SetVoxelComplex(GenerateVoxelItem.Index, GenerateVoxelItem, true, this))
			{
				UEventModuleStatics::BroadcastEvent<UEventHandle_VoxelGenerated>(Cast<UObject>(this), { &GenerateVoxelItem, Cast<UObject>(this) });
				GenerateVoxelItem = FVoxelItem::Empty;
				return true;
			}
			GenerateVoxelItem = FVoxelItem::Empty;
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
			DestroyVoxelItem = InHitResult.VoxelItem;
			DestroyVoxelRemaining = DestroyVoxelItem.GetVoxelData().Hardness;
			return true;
		}
		case EInputInteractEvent::Triggered:
		{
			if(!DestroyVoxelItem.IsValid()) break;
			
			if(!DestroyVoxelItem.EqualIndex(InHitResult.VoxelItem))
			{
				DestroyVoxelItem = InHitResult.VoxelItem;
				DestroyVoxelRemaining = DestroyVoxelItem.GetVoxelData().Hardness;
			}

			DestroyVoxelRemaining -= UCommonStatics::GetCurrentDeltaSeconds() * GetDestroyVoxelRate();
			if(DestroyVoxelRemaining <= 0.f)
			{
				if(Chunk->SetVoxelComplex(DestroyVoxelItem.Index, FVoxelItem::Empty, true, this))
				{
					UEventModuleStatics::BroadcastEvent<UEventHandle_VoxelDestroyed>(Cast<UObject>(this), { &DestroyVoxelItem, Cast<UObject>(this) });
				}
				return true;
			}
			break;
		}
		case EInputInteractEvent::Completed:
		{
			DestroyVoxelItem = FVoxelItem::Empty;
			return true;
		}
	}
	return false;
}

bool IVoxelAgentInterface::OnInteractVoxel(EInputInteractAction InInteractAction, EInputInteractEvent InInteractEvent, const FVoxelHitResult& InHitResult)
{
	return InHitResult.GetVoxel().OnAgentInteract(this, InInteractAction, InInteractEvent, InHitResult);
}

float IVoxelAgentInterface::GetDestroyVoxelProgress() const
{
	if(DestroyVoxelItem.IsValid())
	{
		const float VoxelHardness = DestroyVoxelItem.GetVoxelData().Hardness;
		return (VoxelHardness - DestroyVoxelRemaining) / VoxelHardness;
	}
	return 0.f;
}
