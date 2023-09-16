// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/VoxelInteract.h"

#include "Audio/AudioModuleBPLibrary.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Datas/VoxelData.h"

UVoxelInteract::UVoxelInteract()
{
	
}

void UVoxelInteract::OnReset_Implementation()
{
	Super::OnReset_Implementation();

	bOpened = false;
}

void UVoxelInteract::LoadData(const FString& InData)
{
	bOpened = (bool)FCString::Atoi(*InData);
}

FString UVoxelInteract::ToData()
{
	return FString::FromInt(bOpened);
}

void UVoxelInteract::OnGenerate(IVoxelAgentInterface* InAgent)
{
	Super::OnGenerate(InAgent);
}

void UVoxelInteract::OnDestroy(IVoxelAgentInterface* InAgent)
{
	Super::OnDestroy(InAgent);
}

void UVoxelInteract::OnAgentHit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentHit(InAgent, InHitResult);
}

void UVoxelInteract::OnAgentEnter(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentEnter(InAgent, InHitResult);
}

void UVoxelInteract::OnAgentStay(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentStay(InAgent, InHitResult);
}

void UVoxelInteract::OnAgentExit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentExit(InAgent, InHitResult);
}

bool UVoxelInteract::OnAgentInteract(IVoxelAgentInterface* InAgent, EVoxelInteractType InActionType, const FVoxelHitResult& InHitResult)
{
	switch (InActionType)
	{
		case EVoxelInteractType::Action1:
		{
			return Super::OnAgentInteract(InAgent, InActionType, InHitResult);
		}
		case EVoxelInteractType::Action2:
		{
			Toggle();
			break;
		}
		default: break;
	}
	return false;
}

void UVoxelInteract::Toggle()
{
	if(!bOpened) Open();
	else Close();
}

void UVoxelInteract::Open()
{
	bOpened = true;
	RefreshData();
	if(GetData().bMainPart)
	{
		for(auto Iter : GetItem().GetParts())
		{
			Iter.GetVoxel<ThisClass>().Open();
		}
		GetOwner()->Generate(EPhase::Lesser);
		UAudioModuleBPLibrary::PlaySoundAtLocation(GetData().GetSound(EVoxelSoundType::Interact1), GetLocation());
	}
}

void UVoxelInteract::Close()
{
	bOpened = false;
	RefreshData();
	if(GetData().bMainPart)
	{
		for(auto Iter : GetItem().GetParts())
		{ 
			Iter.GetVoxel<ThisClass>().Close();
		}
		GetOwner()->Generate(EPhase::Lesser);
		UAudioModuleBPLibrary::PlaySoundAtLocation(GetData().GetSound(EVoxelSoundType::Interact2), GetLocation());
	}
}
