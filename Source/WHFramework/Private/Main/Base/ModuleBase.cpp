// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/Base/ModuleBase.h"

#include "Main/MainModule.h"
#include "Net/UnrealNetwork.h"
#include "SaveGame/SaveGameModuleStatics.h"

// Sets default values
UModuleBase::UModuleBase()
{
	ModuleName = NAME_None;
	ModuleDisplayName = FText::GetEmpty();
	ModuleDescription = FText::GetEmpty();
	ModuleState = EModuleState::None;
	bModuleAutoRun = true;
	bModuleAutoSave = false;
	ModuleSaveGame = nullptr;
	ModuleNetworkComponent = nullptr;
}

#if WITH_EDITOR
void UModuleBase::OnGenerate()
{
	
}

void UModuleBase::OnDestroy()
{
	
}
#endif

void UModuleBase::OnInitialize()
{
	K2_OnInitialize();
}

void UModuleBase::OnPreparatory(EPhase InPhase)
{
	K2_OnPreparatory(InPhase);
}

void UModuleBase::OnPause()
{
	K2_OnPause();
}

void UModuleBase::OnUnPause()
{
	K2_OnUnPause();
}

void UModuleBase::OnRefresh(float DeltaSeconds)
{
	K2_OnRefresh(DeltaSeconds);
}

void UModuleBase::OnTermination(EPhase InPhase)
{
	K2_OnTermination(InPhase);
}

void UModuleBase::OnStateChanged(EModuleState InModuleState)
{
	K2_OnStateChanged(InModuleState);
	OnModuleStateChanged.Broadcast(InModuleState);
}

void UModuleBase::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
}

void UModuleBase::UnloadData(EPhase InPhase)
{
	ISaveDataInterface::UnloadData(InPhase);
}

FSaveData* UModuleBase::ToData()
{
	return nullptr;
}

void UModuleBase::OnReset_Implementation()
{
	
}

void UModuleBase::Load_Implementation()
{
	USaveGameModuleStatics::LoadOrCreateSaveGame(ModuleSaveGame, 0);
}

void UModuleBase::Save_Implementation()
{
	USaveGameModuleStatics::SaveSaveGame(ModuleSaveGame, 0, true);
}

void UModuleBase::Run_Implementation()
{
	if(ModuleState == EModuleState::None)
	{
		ModuleState = EModuleState::Running;
		OnStateChanged(ModuleState);
		OnPreparatory(EPhase::Final);
	}
}

void UModuleBase::Reset_Implementation()
{
	Execute_OnReset(this);
}

void UModuleBase::Pause_Implementation()
{
	if(ModuleState != EModuleState::Paused)
	{
		ModuleState = EModuleState::Paused;
		OnStateChanged(ModuleState);
		OnPause();
	}
}

void UModuleBase::UnPause_Implementation()
{
	if(ModuleState == EModuleState::Paused)
	{
		ModuleState = EModuleState::Running;
		OnStateChanged(ModuleState);
		OnUnPause();
	}
}

void UModuleBase::Termination_Implementation()
{
	if(ModuleState != EModuleState::Terminated)
	{
		ModuleState = EModuleState::Terminated;
		OnStateChanged(ModuleState);
		OnTermination(EPhase::Final);
	}
}

bool UModuleBase::IsAutoRunModule() const
{
	return bModuleAutoRun;
}

FName UModuleBase::GetModuleName() const
{
	return ModuleName;
}

FText UModuleBase::GetModuleDisplayName() const
{
	return ModuleDisplayName;
}

FText UModuleBase::GetModuleDescription() const
{
	return ModuleDescription;
}

EModuleState UModuleBase::GetModuleState() const
{
	return ModuleState;
}

AMainModule* UModuleBase::GetOwner() const
{
	return Cast<AMainModule>(GetOuter());
}

USaveGameBase* UModuleBase::GetModuleSaveGame() const
{
	return USaveGameModuleStatics::GetSaveGame(ModuleSaveGame);
}

UModuleNetworkComponentBase* UModuleBase::GetModuleNetworkComponent() const
{
	return UMainModuleStatics::GetModuleNetworkComponentByClass(ModuleNetworkComponent);
}

void UModuleBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UModuleBase, ModuleState);
}

int32 UModuleBase::GetModuleIndex() const
{
	return GetOwner()->GetModules().IndexOfByKey(this);
}

void UModuleBase::GenerateListItem(TSharedPtr<FModuleListItem> OutModuleListItem)
{
	OutModuleListItem->Module = this;
}

void UModuleBase::UpdateListItem(TSharedPtr<FModuleListItem> OutModuleListItem)
{
	OutModuleListItem->Module = this;
}

#if WITH_EDITOR
bool UModuleBase::CanEditChange(const FProperty* InProperty) const
{
	if(InProperty)
	{
		FString PropertyName = InProperty->GetName();

		return true;
	}

	return Super::CanEditChange(InProperty);
}

void UModuleBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FProperty* Property = PropertyChangedEvent.MemberProperty;

	if(Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		auto PropertyName = Property->GetFName();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
