// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/WHPlayerState.h"

AWHPlayerState::AWHPlayerState()
{
	bInitialized = false;
}

void AWHPlayerState::OnInitialize_Implementation()
{
	bInitialized = true;
}

void AWHPlayerState::OnPreparatory_Implementation()
{
	
}

void AWHPlayerState::OnRefresh_Implementation(float DeltaSeconds)
{
	
}

void AWHPlayerState::OnTermination_Implementation()
{
	
}

void AWHPlayerState::BeginPlay()
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

void AWHPlayerState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if(Execute_IsUseDefaultLifecycle(this))
	{
		Execute_OnTermination(this);
	}
}

void AWHPlayerState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if(Execute_IsUseDefaultLifecycle(this))
	{
		Execute_OnRefresh(this, DeltaSeconds);
	}
}
