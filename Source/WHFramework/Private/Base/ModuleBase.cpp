// Fill out your copyright notice in the Description page of Project Settings.


#include "Base/ModuleBase.h"

#include "Net/UnrealNetwork.h"

// Sets default values
AModuleBase::AModuleBase()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	ModuleState = EModuleState::None;

	RootComponent = CreateDefaultSubobject<USceneComponent>(FName("RootComponent"));
}

#if WITH_EDITOR
void AModuleBase::OnGenerate_Implementation()
{
	
}

void AModuleBase::OnDestroy_Implementation()
{
	
}
#endif

void AModuleBase::OnInitialize_Implementation()
{
	ModuleState = EModuleState::Executing;
}

void AModuleBase::OnRefresh_Implementation(float DeltaSeconds)
{
	
}

void AModuleBase::OnPause_Implementation()
{
	ModuleState = EModuleState::Pausing;
}

void AModuleBase::OnUnPause_Implementation()
{
	ModuleState = EModuleState::Executing;
}

void AModuleBase::Pause_Implementation()
{
	if(ModuleState != EModuleState::Pausing)
	{
		Execute_OnPause(this);
	}
}

void AModuleBase::UnPause_Implementation()
{
	if(ModuleState != EModuleState::Executing)
	{
		Execute_OnUnPause(this);
	}
}

void AModuleBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AModuleBase, ModuleName);
	DOREPLIFETIME(AModuleBase, ModuleState);
}
