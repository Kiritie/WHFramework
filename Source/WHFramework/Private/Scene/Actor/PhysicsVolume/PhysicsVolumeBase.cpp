// Fill out your copyright notice in the Description page of Project Settings.


#include "Scene/Actor/PhysicsVolume/PhysicsVolumeBase.h"

#include "TimerManager.h"
#include "Engine/World.h"

APhysicsVolumeBase::APhysicsVolumeBase()
{
	VolumeName = NAME_None;

	bInitialized = false;
}

void APhysicsVolumeBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
}

void APhysicsVolumeBase::OnDespawn_Implementation(bool bRecovery)
{
	VolumeName = NAME_None;
}

void APhysicsVolumeBase::OnInitialize_Implementation()
{
	bInitialized = true;
}

void APhysicsVolumeBase::OnPreparatory_Implementation(EPhase InPhase)
{
	
}

void APhysicsVolumeBase::OnRefresh_Implementation(float DeltaSeconds)
{
	
}

void APhysicsVolumeBase::OnTermination_Implementation(EPhase InPhase)
{
	
}

void APhysicsVolumeBase::BeginPlay()
{
	Super::BeginPlay();

	if(Execute_IsUseDefaultLifecycle(this))
	{
		if(!Execute_IsInitialized(this))
		{
			Execute_OnInitialize(this);
		}
		Execute_OnPreparatory(this, EPhase::Final);
	}
}

void APhysicsVolumeBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if(Execute_IsUseDefaultLifecycle(this))
	{
		Execute_OnTermination(this, EPhase::Final);
	}
}

void APhysicsVolumeBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if(Execute_IsUseDefaultLifecycle(this))
	{
		Execute_OnRefresh(this, DeltaSeconds);
	}
}

void APhysicsVolumeBase::Initialize(const FPhysicsVolumeData& InPhysicsVolumeData)
{
	VolumeName = InPhysicsVolumeData.Name;
	Priority = InPhysicsVolumeData.Priority;
	FluidFriction = InPhysicsVolumeData.FluidFriction;
	TerminalVelocity = InPhysicsVolumeData.TerminalVelocity;
	bWaterVolume = InPhysicsVolumeData.bWaterVolume;
	bPhysicsOnContact = InPhysicsVolumeData.bPhysicsOnContact;
}
