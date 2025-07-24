// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/WHHUD.h"

AWHHUD::AWHHUD()
{
	bInitialized = false;
}

void AWHHUD::OnInitialize_Implementation()
{
	bInitialized = true;
}

void AWHHUD::OnPreparatory_Implementation()
{
	
}

void AWHHUD::OnRefresh_Implementation(float DeltaSeconds)
{
	
}

void AWHHUD::OnTermination_Implementation()
{
	
}

void AWHHUD::BeginPlay()
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

void AWHHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if(Execute_IsUseDefaultLifecycle(this))
	{
		Execute_OnTermination(this);
	}
}

void AWHHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if(Execute_IsUseDefaultLifecycle(this))
	{
		Execute_OnRefresh(this, DeltaSeconds);
	}
}
