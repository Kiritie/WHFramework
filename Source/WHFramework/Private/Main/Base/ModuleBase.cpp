// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/Base/ModuleBase.h"

#include "Main/MainModule.h"
#include "Net/UnrealNetwork.h"
#include "SaveGame/SaveGameModuleStatics.h"

// Sets default values
UModuleBase::UModuleBase()
{
	ModuleName = FName("ModuleBase");
	ModuleDisplayName = FText::FromString("Module Base");
	ModuleDescription = FText::GetEmpty();
	ModuleState = EModuleState::None;
	ModuleIndex = 0;
	bModuleAutoRun = true;
	bModuleAutoSave = false;
	ModuleSavePhase = EPhase::Final;
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

	if(InPhase == ModuleSavePhase)
	{
		Load();
	}
}

void UModuleBase::OnReset()
{
	K2_OnReset();
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

	if(InPhase == ModuleSavePhase)
	{
		Save();
	}
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

void UModuleBase::Load_Implementation()
{
	if(bModuleAutoSave)
	{
		USaveGameModuleStatics::LoadOrCreateSaveGame(ModuleSaveGame, 0);
	}
}

void UModuleBase::Save_Implementation()
{
	if(bModuleAutoSave)
	{
		USaveGameModuleStatics::SaveSaveGame(ModuleSaveGame, 0, true);
	}
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
	OnReset();
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

AMainModule* UModuleBase::GetModuleOwner() const
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

#if WITH_EDITOR
bool UModuleBase::GenerateListItem(TSharedPtr<FModuleListItem> OutModuleListItem, const FString& InFilterText)
{
	OutModuleListItem->Module = this;
	if(!InFilterText.IsEmpty())
	{
		return ModuleDisplayName.ToString().Contains(InFilterText);
	}
	return true;
}

void UModuleBase::UpdateListItem(TSharedPtr<FModuleListItem> OutModuleListItem)
{
	OutModuleListItem->Module = this;
}

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
