// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Voxel.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetManagerBase.h"
#include "Audio/AudioModuleBPLibrary.h"
#include "Voxel/Datas/VoxelData.h"
#include "Math/MathBPLibrary.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleBPLibrary.h"
#include "Voxel/Agent/VoxelAgentInterface.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxel::UVoxel()
{
	Item = FVoxelItem();
}

UVoxel& UVoxel::GetEmpty()
{
	return UReferencePoolModuleBPLibrary::GetReference<UVoxelEmpty>();
}

UVoxel& UVoxel::GetUnknown()
{
	return UReferencePoolModuleBPLibrary::GetReference<UVoxelUnknown>();
}

void UVoxel::OnReset_Implementation()
{
	Item = FVoxelItem();
}

void UVoxel::LoadData(const FString& InData)
{
	
}

FString UVoxel::ToData()
{
	return TEXT("");
}

void UVoxel::RefreshData()
{
	Item.RefreshData(*this, true);
}

void UVoxel::OnGenerate(IVoxelAgentInterface* InAgent)
{
	if(InAgent) return;
	
	if(GetData().bMainPart)
	{
		UAudioModuleBPLibrary::PlaySoundAtLocation(GetData().GetSound(EVoxelSoundType::Generate), GetLocation());
	}
}

void UVoxel::OnDestroy(IVoxelAgentInterface* InAgent)
{
	if(!InAgent) return;
	
	if(GetData().bMainPart)
	{
		UAudioModuleBPLibrary::PlaySoundAtLocation(GetData().GetSound(EVoxelSoundType::Destroy), GetLocation());
		UAbilityModuleBPLibrary::SpawnPickUp(FAbilityItem(GetID(), 1), GetLocation() + GetData().GetRange(GetAngle()) * AVoxelModule::Get()->GetWorldData().BlockSize * 0.5f, GetOwner());
	}
	if(GetOwner())
	{
		TMap<FIndex, FVoxelItem> VoxelItems;
		ITER_ARRAY({ EVoxelType::Water }, WaterType,
			if(GetOwner()->CheckVoxelNeighbors(GetIndex(), WaterType, FVector::OneVector, false, true))
			{
				VoxelItems.Emplace(GetIndex(), UVoxelModuleBPLibrary::VoxelTypeToAssetID(WaterType));
				break;
			}
		)
		if(GetOwner()->HasVoxel(UMathBPLibrary::GetAdjacentIndex(GetIndex(), EDirection::Up)) && !GetOwner()->CheckVoxelAdjacent(Item, EDirection::Up))
		{
			VoxelItems.Emplace(UMathBPLibrary::GetAdjacentIndex(GetIndex(), EDirection::Up), FVoxelItem::Empty);
		}
		GetOwner()->SetVoxelComplex(VoxelItems, true, false, InAgent);
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

bool UVoxel::OnAgentInteract(IVoxelAgentInterface* InAgent, EInputInteractAction InInteractAction, const FVoxelHitResult& InHitResult)
{
	switch (InInteractAction)
	{
		case EInputInteractAction::Action1:
		{
			return InAgent->OnDestroyVoxel(InHitResult);
		}
		case EInputInteractAction::Action2:
		{
			return InAgent->OnGenerateVoxel(InHitResult);
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
