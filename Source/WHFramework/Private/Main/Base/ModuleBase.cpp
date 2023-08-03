// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/Base/ModuleBase.h"

#include "Net/UnrealNetwork.h"

// Sets default values
AModuleBase::AModuleBase()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	bAutoRunModule = true;
	ModuleName = NAME_None;
	ModuleState = EModuleState::None;
}

#if WITH_EDITOR
void AModuleBase::OnGenerate()
{
	
}

void AModuleBase::OnDestroy()
{
	
}
#endif

void AModuleBase::OnInitialize_Implementation()
{
	
}

void AModuleBase::OnPreparatory_Implementation(EPhase InPhase)
{
	
}

void AModuleBase::OnRefresh_Implementation(float DeltaSeconds)
{
	
}

void AModuleBase::OnTermination_Implementation(EPhase InPhase)
{
	
}

void AModuleBase::OnReset_Implementation()
{
	
}

void AModuleBase::OnPause_Implementation()
{
	
}

void AModuleBase::OnUnPause_Implementation()
{
	
}

void AModuleBase::OnStateChanged_Implementation(EModuleState InModuleState)
{
	OnModuleStateChanged.Broadcast(InModuleState);
}

void AModuleBase::Run_Implementation()
{
	if(ModuleState == EModuleState::None)
	{
		ModuleState = EModuleState::Running;
		OnStateChanged(ModuleState);
		Execute_OnPreparatory(this, EPhase::Final);
	}
}

void AModuleBase::Reset_Implementation()
{
	Super::Reset();
	Execute_OnReset(this);
}

void AModuleBase::Pause_Implementation()
{
	if(ModuleState != EModuleState::Paused)
	{
		ModuleState = EModuleState::Paused;
		OnStateChanged(ModuleState);
		OnPause();
	}
}

void AModuleBase::UnPause_Implementation()
{
	if(ModuleState == EModuleState::Paused)
	{
		ModuleState = EModuleState::Running;
		OnStateChanged(ModuleState);
		OnUnPause();
	}
}

void AModuleBase::Termination_Implementation()
{
	if(ModuleState != EModuleState::Terminated)
	{
		ModuleState = EModuleState::Terminated;
		OnStateChanged(ModuleState);
		Execute_OnTermination(this, EPhase::Final);
	}
}

bool AModuleBase::IsAutoRunModule() const
{
	return bAutoRunModule;
}

FName AModuleBase::GetModuleName() const
{
	return ModuleName;
}

EModuleState AModuleBase::GetModuleState() const
{
	return ModuleState;
}

void AModuleBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AModuleBase, ModuleState);
}
