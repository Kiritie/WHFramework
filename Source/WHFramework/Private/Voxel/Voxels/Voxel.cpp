// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Voxel.h"

#include "Ability/AbilityModuleStatics.h"
#include "Audio/AudioModuleStatics.h"
#include "Event/EventModuleStatics.h"
#include "Event/Handle/Voxel/EventHandle_VoxelDestroyed.h"
#include "Event/Handle/Voxel/EventHandle_VoxelGenerated.h"
#include "Math/MathHelper.h"
#include "Voxel/Voxels/Data/VoxelData.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Agent/VoxelAgentInterface.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxel::UVoxel()
{
	Item = FVoxelItem();
}

UVoxel& UVoxel::GetEmpty()
{
	return UReferencePoolModuleStatics::GetReference<UVoxelEmpty>();
}

UVoxel& UVoxel::GetUnknown()
{
	return UReferencePoolModuleStatics::GetReference<UVoxelUnknown>();
}

void UVoxel::OnReset_Implementation()
{
	Item = FVoxelItem();
}

void UVoxel::LoadData(const FString& InData)
{
	Item.Data = InData;
}

FString UVoxel::ToData()
{
	return Item.Data;
}

void UVoxel::RefreshData()
{
	Item.RefreshData(*this, true);
}

void UVoxel::OnGenerate(IVoxelAgentInterface* InAgent)
{
	if(!InAgent) return;

	if(GetData().IsMainPart())
	{
		UAudioModuleStatics::PlaySoundAtLocation(GetData().GetSound(EVoxelSoundType::Generate), GetLocation());
		FVoxelItemParameterValue ItemValue;
		ItemValue.Value = Item;
		UEventModuleStatics::BroadcastEvent<UEventHandle_VoxelGenerated>(Cast<UObject>(InAgent), { MoveTemp(ItemValue), Cast<UObject>(InAgent) });
	}
}

void UVoxel::OnDestroy(IVoxelAgentInterface* InAgent)
{
	if(!InAgent) return;
	
	if(GetData().IsMainPart())
	{
		UAudioModuleStatics::PlaySoundAtLocation(GetData().GetSound(EVoxelSoundType::Destroy), GetLocation());
		const bool bCanPickUp = Item.GetVoxelType() != EVoxelType::Water || FVoxelLiquidState(Item.Data).IsSource();
		if(bCanPickUp && UVoxelModuleStatics::GetVoxelWorldMode() != EVoxelWorldMode::Prefab)
		{
			UAbilityModuleStatics::SpawnAbilityPickUp(FAbilityItem(GetData().GatherData ? GetData().GatherData->GetPrimaryAssetId() : GetData().GetPrimaryAssetId(), 1), GetLocation() + GetData().GetRange(GetAngle()) * UVoxelModule::Get().GetWorldData().BlockSize * 0.5f, GetOwner());
		}
		FVoxelItemParameterValue ItemValue;
		ItemValue.Value = Item;
		UEventModuleStatics::BroadcastEvent<UEventHandle_VoxelDestroyed>(Cast<UObject>(InAgent), { MoveTemp(ItemValue), Cast<UObject>(InAgent) });
	}
	if(GetOwner() && (Item.GetVoxelType() == EVoxelType::Oak || Item.GetVoxelType() == EVoxelType::Birch) && Item.Data.StartsWith(TEXT("R")))
	{
		GetOwner()->DestroyTree(GetIndex());
	}
	if(GetOwner())
	{
		const FIndex UpperIndex = FMathHelper::GetAdjacentIndex(GetIndex(), EDirectionN::Up);
		if(GetOwner()->HasVoxelComplex(UpperIndex) && GetOwner()->GetVoxelComplex(UpperIndex).GetData().GetTransparency() == EVoxelTransparency::Trans)
		{
			GetOwner()->SetVoxelComplex(UpperIndex, FVoxelItem::Empty, true, InAgent);
		}
	}
}

void UVoxel::OnAgentHit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	
}

void UVoxel::OnAgentEnter(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	
}

void UVoxel::OnAgentStay(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	
}

void UVoxel::OnAgentExit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	
}

bool UVoxel::OnAgentInteract(IVoxelAgentInterface* InAgent, EInputInteractAction InInteractAction, EInputInteractEvent InInteractEvent, const FVoxelHitResult& InHitResult)
{
	switch (InInteractAction)
	{
		case EInputInteractAction::Primary:
		{
			return InAgent->OnDestroyVoxel(InInteractEvent, InHitResult);
		}
		case EInputInteractAction::Secondary:
		{
			return InAgent->OnGenerateVoxel(InInteractEvent, InHitResult);
		}
		case EInputInteractAction::Third:
		{
			break;
		}
		default: break;
	}
	return false;
}

bool UVoxel::IsValid() const
{
	return !IsEmpty() && !IsUnknown();
}

bool UVoxel::IsEmpty() const
{
	return &UVoxel::GetEmpty() == this;
}

bool UVoxel::IsUnknown() const
{
	return &UVoxel::GetUnknown() == this;
}

UVoxelChunk* UVoxel::GetOwner() const
{
	return Item.Chunk;
}
