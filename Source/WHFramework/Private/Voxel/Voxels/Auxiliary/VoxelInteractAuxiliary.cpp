
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
	BoxComponent = CreateDefaultSubobject<UBoxComponent>(FName("BoxComponent"));
	BoxComponent->SetupAttachment(RootComponent);
	BoxComponent->SetCollisionProfileName(TEXT("VoxelAuxiliary"));

	Interaction = CreateDefaultSubobject<UInteractionComponent>(FName("Interaction"));
	Interaction->SetupAttachment(RootComponent);
	Interaction->SetInteractable(false);
}

void AVoxelInteractAuxiliary::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);

	Interaction->ClearInteractActions();
}

void AVoxelInteractAuxiliary::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	Super::LoadData(InSaveData, InPhase);

	const auto& SaveData = InSaveData->CastRef<FVoxelAuxiliarySaveData>();

	if(PHASEC(InPhase, EPhase::All))
	{
		switch(SaveData.MeshNature)
		{
			case EVoxelScope::Chunk:
			{
				for(const auto& Iter : VoxelItem.GetVoxelData<UVoxelInteractData>().InteractActions)
				{
					Interaction->AddInteractAction((EInteractAction)Iter);
				}
				BoxComponent->SetGenerateOverlapEvents(true);
				BoxComponent->SetBoxExtent(VoxelItem.GetRange() * UVoxelModule::Get().GetWorldData().BlockSize * 0.5f);
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
			return true;
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
				GetVoxelItem().GetVoxel<UVoxelInteract>().Interact(nullptr);
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
