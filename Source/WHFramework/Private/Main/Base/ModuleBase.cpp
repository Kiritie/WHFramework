// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/Base/ModuleBase.h"
#include "Common/LifecycleTypes.h"

#include "Main/MainModule.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/WHGameInstance.h"
#include "Net/UnrealNetwork.h"
#include "SaveGame/SaveDataSerializer.h"

// Sets default values
UModuleBase::UModuleBase()
{
	ModuleName = FName("ModuleBase");
	ModuleDisplayName = FText::FromString("Module Base");
	ModuleDescription = FText::GetEmpty();
	ModuleState = EModuleState::None;
	ModuleIndex = 0;
	bModuleRequired = false;
	bModuleAutoRun = true;
	SaveScope = ESaveScope::None;
	SaveDataVersion = 1;
	ModuleNetworkComponent = nullptr;
	ModuleDependencies = TArray<FName>();
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

void UModuleBase::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	K2_OnRefresh(DeltaSeconds, bInEditor);
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

void UModuleBase::LoadData(const FParameter& InSaveData, EPhase InPhase)
{
}

void UModuleBase::UnloadData(EPhase InPhase)
{
	ISaveDataAgentInterface::UnloadData(InPhase);
}

FParameter UModuleBase::ToData()
{
	return FParameter();
}

bool UModuleBase::BuildSaveFile(TArray<uint8>& OutBytes)
{
	if(!IsSaveEnabled())
	{
		return false;
	}

	FParameter Data = GetSaveData(true);
	return Data.HasValue() && FSaveDataSerializer::BuildModuleFile(ModuleName, GetSaveDataVersion(), Data, OutBytes);
}

void UModuleBase::Load_Implementation()
{
}

void UModuleBase::Save_Implementation()
{
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
	switch(ModuleState)
	{
		case EModuleState::None:
		{
			ModuleState = EModuleState::UnRunning;
			OnStateChanged(ModuleState);
			OnPause();
			break;
		}
		case EModuleState::Running:
		{
			ModuleState = EModuleState::Paused;
			OnStateChanged(ModuleState);
			OnPause();
			break;
		}
		default: break;
	}
}

void UModuleBase::UnPause_Implementation()
{
	switch(ModuleState)
	{
		case EModuleState::UnRunning:
		{
			ModuleState = EModuleState::Running;
			OnStateChanged(ModuleState);
			OnPreparatory(EPhase::Final);
			OnUnPause();
			break;
		}
		case EModuleState::Paused:
		{
			ModuleState = EModuleState::Running;
			OnStateChanged(ModuleState);
			OnUnPause();
			break;
		}
		default: break;
	}
}

void UModuleBase::BeginHostTermination()
{
	CloseHostBusinessAdmission();
	bHostTerminationActive = true;
}

void UModuleBase::TerminatePhaseFromHost(EPhase InPhase)
{
	if (ModuleState == EModuleState::Terminated) return;
	if (InPhase == EPhase::Final)
	{
		ModuleState = EModuleState::Terminated;
		OnStateChanged(ModuleState);
	}
	OnTermination(InPhase);
}

void UModuleBase::Termination()
{
	if (bHostTerminationActive || ModuleState == EModuleState::Terminated) return;
	BeginHostTermination();
	PrepareForHostTermination();
	TerminatePhaseFromHost(EPhase::Primary);
	TerminatePhaseFromHost(EPhase::Lesser);
	TerminatePhaseFromHost(EPhase::Final);
}

bool UModuleBase::IsModuleInEditor() const
{
	const AMainModule* Owner = GetModuleOwner();
	return Owner && Owner->IsInEditor();
}

AMainModule* UModuleBase::GetModuleOwner() const
{
	return Cast<AMainModule>(GetOuter());
}

UWHGameInstance* UModuleBase::GetGameInstanceOwner() const
{
	return Cast<UWHGameInstance>(GetOuter());
}

UModuleNetworkComponentBase* UModuleBase::GetModuleNetworkComponent(APlayerController* InController) const
{
	return InController && InController->GetWorld() == GetWorld()
		? AMainModule::GetModuleNetworkComponentByClass<UModuleNetworkComponentBase>(InController, ModuleNetworkComponent)
		: nullptr;
}

void UModuleBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UModuleBase, ModuleState);
}

FString UModuleBase::GetModuleDebugMessage()
{
	return TEXT("No message...");
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
		const FString PropertyName = InProperty->GetName();

		return true;
	}

	return Super::CanEditChange(InProperty);
}

void UModuleBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FProperty* Property = PropertyChangedEvent.MemberProperty;

	if(Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		const FName PropertyName = Property->GetFName();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
