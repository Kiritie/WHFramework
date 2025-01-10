// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/VoxelInteract.h"

#include "Audio/AudioModuleStatics.h"
#include "Voxel/Agent/VoxelAgentInterface.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Datas/VoxelData.h"
#include "Voxel/Voxels/Auxiliary/VoxelInteractAuxiliary.h"

UVoxelInteract::UVoxelInteract()
{
	bOpened = false;
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

bool UVoxelInteract::OnAgentInteract(IVoxelAgentInterface* InAgent, EInputInteractAction InInteractAction, EInputInteractEvent InInteractEvent, const FVoxelHitResult& InHitResult)
{
	switch (InInteractAction)
	{
		case EInputInteractAction::Primary:
		{
			return Super::OnAgentInteract(InAgent, InInteractAction, InInteractEvent, InHitResult);
		}
		case EInputInteractAction::Secondary:
		{
			if(InInteractEvent == EInputInteractEvent::Started)
			{
				Toggle(InAgent);
			}
			return true;
		}
		default: break;
	}
	return false;
}

void UVoxelInteract::Toggle(IVoxelAgentInterface* InAgent)
{
	if(!bOpened) Open(InAgent);
	else Close(InAgent);
}

void UVoxelInteract::Open(IVoxelAgentInterface* InAgent)
{
	if(IInteractionAgentInterface* InteractionAgent = Cast<IInteractionAgentInterface>(InAgent))
	{
		if(AVoxelInteractAuxiliary* InteractAuxiliary = Cast<AVoxelInteractAuxiliary>(GetItem().Auxiliary))
		{
			InteractionAgent->SetInteractingAgent(InteractAuxiliary);
			InteractionAgent->DoInteract((EInteractAction)EVoxelInteractAction::Open, InteractAuxiliary);
		}
	}
	SetOpened(true);
	if(GetData().IsMainPart())
	{
		for(auto& Iter : GetItem().GetParts())
		{
			Iter.GetVoxel<ThisClass>().Open(InAgent);
		}
		UAudioModuleStatics::PlaySoundAtLocation(GetData().GetSound(EVoxelSoundType::Open), GetLocation());
	}
	GetOwner()->Generate(EPhase::Lesser);
}

void UVoxelInteract::Close(IVoxelAgentInterface* InAgent)
{
	if(IInteractionAgentInterface* InteractionAgent = Cast<IInteractionAgentInterface>(InAgent))
	{
		if(AVoxelInteractAuxiliary* InteractAuxiliary = Cast<AVoxelInteractAuxiliary>(GetItem().Auxiliary))
		{
			InteractionAgent->SetInteractingAgent(InteractAuxiliary);
			InteractionAgent->DoInteract((EInteractAction)EVoxelInteractAction::Close, InteractAuxiliary);
		}
	}
	SetOpened(false);
	if(GetData().IsMainPart())
	{
		for(auto& Iter : GetItem().GetParts())
		{ 
			Iter.GetVoxel<ThisClass>().Close(InAgent);
		}
		UAudioModuleStatics::PlaySoundAtLocation(GetData().GetSound(EVoxelSoundType::Close), GetLocation());
	}
	GetOwner()->Generate(EPhase::Lesser);
}
