
// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Auxiliary/VoxelSwitchAuxiliary.h"

#include "Common/Interaction/InteractionComponent.h"
#include "Voxel/Interaction/VoxelInteractionOptions.h"

// Sets default values
AVoxelSwitchAuxiliary::AVoxelSwitchAuxiliary()
{
	Interaction->Options.Reset();
	Interaction->Options.Add(CreateDefaultSubobject<UInteractionOption_VoxelOpen>(TEXT("OpenOption")));
	Interaction->Options.Add(CreateDefaultSubobject<UInteractionOption_VoxelClose>(TEXT("CloseOption")));
}
