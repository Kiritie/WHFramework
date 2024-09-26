
// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Auxiliary/VoxelInteractAuxiliary.h"

#include "Common/Interaction/InteractionComponent.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Agent/VoxelAgentInterface.h"
#include "Voxel/Datas/VoxelInteractData.h"
#include "Voxel/Voxels/VoxelInteract.h"

// Sets default values
AVoxelInteractAuxiliary::AVoxelInteractAuxiliary()
{
	BoxComponent = CreateDefaultSubobject<UInteractionComponent>(FName("BoxComponent"));
	BoxComponent->SetupAttachment(RootComponent);
	BoxComponent->SetCollisionProfileName(TEXT("VoxelAuxiliary"));

	Interaction = CreateDefaultSubobject<UInteractionComponent>(FName("Interaction"));
	Interaction->SetupAttachment(RootComponent);
	Interaction->SetInteractable(false);
}

void AVoxelInteractAuxiliary::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);

	Interaction->SetInteractable(false);
	Interaction->ClearInteractActions();
}

void AVoxelInteractAuxiliary::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	Super::LoadData(InSaveData, InPhase);
	
	if(PHASEC(InPhase, EPhase::All))
	{
		for(const auto& Iter : VoxelItem.GetVoxelData<UVoxelInteractData>().InteractActions)
		{
			Interaction->AddInteractAction(Iter);
		}
		Interaction->SetInteractable(VoxelItem.Owner != nullptr);
		BoxComponent->SetBoxExtent(VoxelItem.GetRange() * UVoxelModule::Get().GetWorldData().BlockSize * 0.5f);
	}
}

bool AVoxelInteractAuxiliary::CanInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent)
{
	switch ((EVoxelInteractAction)InInteractAction)
	{
		case EVoxelInteractAction::Open:
		{
			if(Cast<IVoxelAgentInterface>(InInteractionAgent))
			{
				return !GetVoxelItem(true).GetVoxel<UVoxelInteract>().IsOpened();
			}
		}
		case EVoxelInteractAction::Close:
		{
			if(Cast<IVoxelAgentInterface>(InInteractionAgent))
			{
				return GetVoxelItem(true).GetVoxel<UVoxelInteract>().IsOpened();
			}
		}
		default: break;
	}
	return false;
}

void AVoxelInteractAuxiliary::OnEnterInteract(IInteractionAgentInterface* InInteractionAgent)
{
}

void AVoxelInteractAuxiliary::OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent)
{
}

void AVoxelInteractAuxiliary::OnInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent, bool bPassivity)
{
	if(bPassivity)
	{
		switch ((EVoxelInteractAction)InInteractAction)
		{
			case EVoxelInteractAction::Open:
			{
				GetVoxelItem().GetVoxel<UVoxelInteract>().Open(nullptr);
				break;
			}
			case EVoxelInteractAction::Close:
			{
				GetVoxelItem().GetVoxel<UVoxelInteract>().Close(nullptr);
				break;
			}
			default: break;
		}
	}
}

UInteractionComponent* AVoxelInteractAuxiliary::GetInteractionComponent() const
{
	return Interaction;
}
