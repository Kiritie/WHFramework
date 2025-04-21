#include "Voxel/Agent/VoxelAgentInterface.h"

#include "Common/CommonStatics.h"
#include "Event/EventModuleStatics.h"
#include "Event/Handle/Voxel/EventHandle_VoxelDestroyed.h"
#include "Event/Handle/Voxel/EventHandle_VoxelGenerated.h"
#include "Math/MathHelper.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Voxels/Data/VoxelData.h"
#include "Voxel/Voxels/Voxel.h"
#include "Voxel/Voxels/Auxiliary/VoxelAuxiliary.h"
#include "Voxel/Voxels/Entity/VoxelEntityPreview.h"

IVoxelAgentInterface::IVoxelAgentInterface()
{
	bCanGenerateVoxel = false;
	GenerateVoxelItem = FVoxelItem::Empty;
	GeneratePreviewVoxel = nullptr;

	DestroyVoxelItem = FVoxelItem::Empty;
}

bool IVoxelAgentInterface::OnGenerateVoxel(EInputInteractEvent InInteractEvent, const FVoxelHitResult& InHitResult)
{
	switch(InInteractEvent)
	{
		case EInputInteractEvent::Started:
		{
			bCanGenerateVoxel = false;
			GenerateVoxelItem = GetGenerateVoxelID();
			return true;
		}
		case EInputInteractEvent::Triggered:
		{
			if(!GenerateVoxelItem.IsValid()) break;

			GenerateVoxelItem.ID = GetGenerateVoxelID();

			if(!GenerateVoxelItem.IsValid()) break;

			GenerateVoxelItem.Owner = InHitResult.GetChunk();
			GenerateVoxelItem.Index = InHitResult.GetChunk()->LocationToIndex(InHitResult.Point - UVoxelModule::Get().GetWorldData().GetBlockSizedNormal(InHitResult.Normal)) + FIndex(InHitResult.Normal);

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
					const ERightAngle Angle = FMathHelper::FloatToRightAngle((GenerateVoxelItem.GetLocation() + UVoxelModule::Get().GetWorldData().BlockSize * 0.5f - GetVoxelAgentLocation()).GetSafeNormal2D().ToOrientationRotator().Yaw);
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

			if(InHitResult.GetChunk()->SetVoxelComplex(GenerateVoxelItem.Index, GenerateVoxelItem, true, this))
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
	switch(InInteractEvent)
	{
		case EInputInteractEvent::Started:
		{
			if(InHitResult.VoxelItem.GetIndex().Z > 0)
			{
				DestroyVoxelItem = InHitResult.VoxelItem;
			}
			return true;
		}
		case EInputInteractEvent::Triggered:
		{
			if(!DestroyVoxelItem.IsValid()) break;

			if(UVoxelModuleStatics::GetVoxelWorldMode() != EVoxelWorldMode::Default)
			{
				DestroyVoxelItem.Durability = 0.f;
			}
			else
			{
				if(!DestroyVoxelItem.EqualIndex(InHitResult.VoxelItem))
				{
					DestroyVoxelItem = InHitResult.VoxelItem;
				}
				DestroyVoxelItem.Durability -= UCommonStatics::GetCurrentDeltaSeconds() * GetDestroyVoxelRate() * (GetGenerateToolType() != EVoxelGenerateToolType::None && (int32)GetGenerateToolType() == (int32)DestroyVoxelItem.GetVoxelData().Element ? 1.7f : 1.f) / DestroyVoxelItem.GetVoxelData().Hardness;
			}
			if(DestroyVoxelItem.Durability <= 0.f)
			{
				if(InHitResult.GetChunk()->SetVoxelComplex(DestroyVoxelItem.Index, FVoxelItem::Empty, true, this))
				{
					UEventModuleStatics::BroadcastEvent<UEventHandle_VoxelDestroyed>(Cast<UObject>(this), { &DestroyVoxelItem, Cast<UObject>(this) });
				}
				if(UVoxelModuleStatics::GetVoxelWorldMode() != EVoxelWorldMode::Default)
				{
					DestroyVoxelItem = FVoxelItem::Empty;
				}
				return true;
			}
			InHitResult.GetChunk()->SetVoxelComplex(DestroyVoxelItem.Index, DestroyVoxelItem, false, this);
			break;
		}
		case EInputInteractEvent::Completed:
		{
			if(!DestroyVoxelItem.IsValid()) break;
			
			DestroyVoxelItem.Durability = 1.f;
			InHitResult.GetChunk()->SetVoxelComplex(DestroyVoxelItem.Index, DestroyVoxelItem, false, this);
			DestroyVoxelItem = FVoxelItem::Empty;
			return true;
		}
	}
	return false;
}

bool IVoxelAgentInterface::InteractVoxel(EInputInteractAction InInteractAction, EInputInteractEvent InInteractEvent, const FVoxelHitResult& InHitResult)
{
	return InHitResult.GetVoxel().OnAgentInteract(this, InInteractAction, InInteractEvent, InHitResult);
}

void IVoxelAgentInterface::UnInteractVoxel(EInputInteractAction InInteractAction, EInputInteractEvent InInteractEvent)
{
	switch (InInteractAction)
	{
		case EInputInteractAction::Primary:
		{
			OnDestroyVoxel(InInteractEvent, FVoxelHitResult());
		}
		case EInputInteractAction::Secondary:
		{
			bCanGenerateVoxel = false;
			OnGenerateVoxel(InInteractEvent, FVoxelHitResult());
		}
		case EInputInteractAction::Third:
		{
			break;
		}
		default: break;
	}
}
