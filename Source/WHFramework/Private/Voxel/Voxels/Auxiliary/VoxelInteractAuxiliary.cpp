
// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Auxiliary/VoxelInteractAuxiliary.h"

#include "Common/Interaction/InteractionComponent.h"
#include "Voxel/Interaction/VoxelInteractionOptions.h"
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
	Interaction->Options.Add(CreateDefaultSubobject<UInteractionOption_VoxelInteract>(TEXT("InteractOption")));
	Interaction->Options.Add(CreateDefaultSubobject<UInteractionOption_VoxelUnInteract>(TEXT("UnInteractOption")));
	
	bInteracting = false;
}

void AVoxelInteractAuxiliary::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	if(bInteracting && InteractingAgent)
	{
		SetInteracting(false, Cast<IVoxelAgentInterface>(Cast<AActor>(InteractingAgent)));
	}

	Super::OnDespawn_Implementation(InMode);
}

void AVoxelInteractAuxiliary::LoadData(const FParameter& InSaveData, EPhase InPhase)
{
	Super::LoadData(InSaveData, InPhase);

	const auto& SaveData = InSaveData.GetRef<FVoxelAuxiliarySaveData>();

	if(PHASEC(InPhase, EPhase::All))
	{
		BoxComponent->SetBoxExtent(VoxelItem.GetRange(false) * UVoxelModule::Get().GetWorldData().BlockSize * 0.5f);
		switch(SaveData.VoxelScope)
		{
			case EVoxelScope::Chunk:
			case EVoxelScope::Prefab:
			{
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

void AVoxelInteractAuxiliary::OnEnterInteract(IInteractionAgentInterface* InInteractionAgent)
{
}

void AVoxelInteractAuxiliary::OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent)
{
}

UInteractionComponent* AVoxelInteractAuxiliary::GetInteractionComponent() const
{
	return Interaction;
}

bool AVoxelInteractAuxiliary::SetInteracting(bool bValue, IVoxelAgentInterface* InInteractionAgent)
{
	if (bInteracting == bValue) return false;
	bInteracting = bValue;
	if (InInteractionAgent)
	{
		if (bInteracting) InInteractionAgent->OnVoxelInteract(this);
		else InInteractionAgent->OnVoxelUnInteract(this);
	}
	Interaction->NotifyOptionsChanged();
	return true;
}
