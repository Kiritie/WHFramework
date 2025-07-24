// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Gameplay/WHGameState.h"

AWHGameState::AWHGameState()
{
	bInitialized = false;
}

void AWHGameState::OnInitialize_Implementation()
{
	bInitialized = true;
}

void AWHGameState::OnPreparatory_Implementation()
{
	
}

void AWHGameState::OnRefresh_Implementation(float DeltaSeconds)
{
	
}

void AWHGameState::OnTermination_Implementation()
{
	
}

void AWHGameState::BeginPlay()
{
	Super::BeginPlay();

	if(Execute_IsUseDefaultLifecycle(this))
	{
		if(!Execute_IsInitialized(this))
		{
			Execute_OnInitialize(this);
		}
		Execute_OnPreparatory(this);
	}
}

void AWHGameState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if(Execute_IsUseDefaultLifecycle(this))
	{
		Execute_OnTermination(this);
	}
}

void AWHGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if(Execute_IsUseDefaultLifecycle(this))
	{
		Execute_OnRefresh(this, DeltaSeconds);
	}
}
