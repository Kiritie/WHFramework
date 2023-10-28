// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/Base/ModuleBase.h"

#include "Net/UnrealNetwork.h"
#include "SaveGame/SaveGameModuleBPLibrary.h"

// Sets default values
AModuleBase::AModuleBase()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	ModuleName = NAME_None;
	ModuleState = EModuleState::None;
	bModuleAutoRun = true;
	bModuleAutoSave = false;
	ModuleSaveGame = nullptr;
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

void AModuleBase::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
}

void AModuleBase::UnloadData(EPhase InPhase)
{
	ISaveDataInterface::UnloadData(InPhase);
}

FSaveData* AModuleBase::ToData()
{
	return nullptr;
}

void AModuleBase::OnReset_Implementation()
{
	
}

void AModuleBase::Load_Implementation()
{
	USaveGameModuleBPLibrary::LoadOrCreateSaveGame(ModuleSaveGame, 0);
}

void AModuleBase::Save_Implementation()
{
	USaveGameModuleBPLibrary::SaveSaveGame(ModuleSaveGame, 0, true);
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
	return bModuleAutoRun;
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
