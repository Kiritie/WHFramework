// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Gameplay/WHGameState.h"

AWHGameState::AWHGameState()
{
	
}

void AWHGameState::OnInitialize_Implementation()
{
	
}

void AWHGameState::OnPreparatory_Implementation()
{
	
}

void AWHGameState::BeginPlay()
{
	Super::BeginPlay();
	OnPreparatory();
}
