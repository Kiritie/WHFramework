
// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Auxiliary/VoxelInteractAuxiliary.h"

#include "Common/Interaction/InteractionComponent.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Agent/VoxelAgentInterface.h"
#include "Voxel/Voxels/Data/VoxelInteractData.h"
#include "Voxel/Voxels/VoxelInteract.h"

// Sets default values
AVoxelInteractAuxiliary::AVoxelInteractAuxiliary()
{
	BoxComponent = CreateDefaultSubobject<UBoxComponent>(FName("BoxComponent"));
	BoxComponent->SetupAttachment(RootComponent);
	BoxComponent->SetCollisionProfileName(TEXT("VoxelAuxiliary"));

	Interaction = CreateDefaultSubobject<UInteractionComponent>(FName("Interaction"));
	Interaction->SetupAttachment(RootComponent);
	Interaction->SetInteractable(false);
	
	bInteracting = false;
}

void AVoxelInteractAuxiliary::OnDespawn_Implementation(bool bRecovery)
{
	if(bInteracting && InteractingAgent)
	{
		InteractingAgent->DoInteract((EInteractAction)EVoxelInteractAction::UnInteract);
	}

	Super::OnDespawn_Implementation(bRecovery);

	Interaction->ClearInteractActions();
}

void AVoxelInteractAuxiliary::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	Super::LoadData(InSaveData, InPhase);

	const auto& SaveData = InSaveData->CastRef<FVoxelAuxiliarySaveData>();

	if(PHASEC(InPhase, EPhase::All))
	{
		BoxComponent->SetBoxExtent(VoxelItem.GetRange(false) * UVoxelModule::Get().GetWorldData().BlockSize * 0.5f);
		switch(SaveData.VoxelScope)
		{
			case EVoxelScope::Chunk:
			case EVoxelScope::Prefab:
			{
				for(const auto& Iter : VoxelItem.GetVoxelData<UVoxelInteractData>().InteractActions)
				{
					Interaction->AddInteractAction((EInteractAction)Iter);
				}
				BoxComponent->SetGenerateOverlapEvents(true);
				break;
			}
			default:
			{
				BoxComponent->SetGenerateOverlapEvents(false);
				break;
			}
		}
	}
}

bool AVoxelInteractAuxiliary::CanInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent)
{
	switch ((EVoxelInteractAction)InInteractAction)
	{
		case EVoxelInteractAction::Interact:
		{
			return !bInteracting;
		}
		case EVoxelInteractAction::UnInteract:
		{
			return bInteracting;
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

void AVoxelInteractAuxiliary::OnInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent, bool bPassive)
{
	if(bPassive)
	{
		switch ((EVoxelInteractAction)InInteractAction)
		{
			case EVoxelInteractAction::Interact:
			{
				if(!GetVoxelItem().GetVoxel<UVoxelInteract>().Interact(nullptr))
				{
					bInteracting = true;
				}
				break;
			}
			case EVoxelInteractAction::UnInteract:
			{
				if(bInteracting)
				{
					bInteracting = false;
					GetVoxelItem().GetVoxel<UVoxelInteract>().UnInteract(nullptr);
				}
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
