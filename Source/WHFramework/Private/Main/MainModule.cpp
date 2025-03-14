// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/MainModule.h"

#include "Ability/AbilityModule.h"
#include "Common/CommonStatics.h"
#include "Event/EventModuleStatics.h"
#include "Event/Handle/Common/Game/EventHandle_GameExited.h"
#include "Event/Handle/Common/Game/EventHandle_GameInited.h"
#include "Event/Handle/Common/Game/EventHandle_GameStarted.h"
#include "Net/UnrealNetwork.h"

IMPLEMENTATION_MAIN_MODULE(AMainModule)

// ParamSets default values
AMainModule::AMainModule()
{
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif

	Modules = TArray<UModuleBase*>();
	ModuleMap = TMap<FName, UModuleBase*>();
}

AMainModule::~AMainModule()
{
	TERMINATION_MAIN_MODULE(AMainModule)
}

#if WITH_EDITOR
void AMainModule::OnGenerate()
{
}

void AMainModule::OnDestroy()
{
	for(auto Iter : Modules)
	{
		Iter->OnDestroy();
	}
	Modules.Empty();
}
#endif

void AMainModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();

	GWorldContext = this;

	for(auto Iter : Modules)
	{
		Iter->OnInitialize();
	}

	UEventModuleStatics::BroadcastEvent<UEventHandle_GameInited>(this);
}

void AMainModule::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();

	ITER_PHASE(Phase,
		for(auto Iter : Modules)
		{
			if(Phase != EPhase::Final)
			{
				Iter->OnPreparatory(Phase);
			}
			else if(Iter->IsModuleAutoRun())
			{
				Iter->Run();
			}
		}
	)

	UEventModuleStatics::BroadcastEvent<UEventHandle_GameStarted>(this);
}

void AMainModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);

	for(auto Iter : Modules)
	{
		if(IsInEditor() || Iter->GetModuleState() == EModuleState::Running)
		{
			Iter->OnRefresh(DeltaSeconds, IsInEditor());
		}
	}
}

void AMainModule::OnTermination_Implementation()
{
	Super::OnTermination_Implementation();
		
	ITER_PHASE(Phase,
		for(auto Iter : Modules)
		{
			if(Phase != EPhase::Final)
			{
				Iter->OnTermination(Phase);
			}
			else
			{
				Iter->Termination();
			}
		}
	)
	
	UEventModuleStatics::BroadcastEvent<UEventHandle_GameExited>(this);

	ModuleMap.Empty();
}

void AMainModule::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	Execute_OnTermination(this);
}

void AMainModule::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Execute_OnRefresh(this, DeltaSeconds);
}

ETickableTickType AMainModule::GetTickableTickType() const
{
	return
#if WITH_EDITOR
		IsTickable() ? ETickableTickType::Conditional : 
#endif // WITH_EDITOR
		ETickableTickType::Always;
}

TStatId AMainModule::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(AMainModule, STATGROUP_Tickables);
}

#if WITH_EDITOR
void AMainModule::GenerateModules()
{
	ModuleMap.Empty();
	for(auto Iter : Modules)
	{
		Iter->OnGenerate();
		ModuleMap.Add(Iter->GetModuleName(), Iter);
	}
	Modify();
}

void AMainModule::DestroyModules()
{
	for(auto Iter : Modules)
	{
		Iter->OnDestroy();
	}
	Modules.Empty();
	ModuleMap.Empty();
	Modify();
}
#endif

void AMainModule::PauseModules_Implementation()
{
	for(auto Iter : Modules)
	{
		Iter->Pause();
	}
}

void AMainModule::UnPauseModules_Implementation()
{
	for(auto Iter : Modules)
	{
		Iter->UnPause();
	}
}

UModuleNetworkComponentBase* AMainModule::GetModuleNetworkComponent(TSubclassOf<UModuleNetworkComponentBase> InClass)
{
	if(const AWHPlayerController* PlayerController = UCommonStatics::GetPlayerController<AWHPlayerController>())
	{
		return Cast<UModuleNetworkComponentBase>(PlayerController->GetComponentByClass(InClass));
	}
	return nullptr;
}

void AMainModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMainModule, Modules);
}

#if WITH_EDITOR
void AMainModule::GenerateModuleListItem(TArray<TSharedPtr<FModuleListItem>>& OutModuleListItems, const FString& InFilterText)
{
	OutModuleListItems = TArray<TSharedPtr<FModuleListItem>>();
	for(auto Iter : Modules)
	{
		auto Item = MakeShared<FModuleListItem>();
		if(Iter->GenerateListItem(Item, InFilterText))
		{
			OutModuleListItems.Add(Item);
		}
	}
}

void AMainModule::UpdateModuleListItem(TArray<TSharedPtr<FModuleListItem>>& OutModuleListItems)
{
	for (int32 i = 0; i < Modules.Num(); i++)
	{
		Modules[i]->ModuleIndex = i;
		Modules[i]->UpdateListItem(OutModuleListItems[i]);
	}
}

bool AMainModule::CanAddModule(TSubclassOf<UModuleBase> InModuleClass) const
{
	return !ModuleMap.Contains(InModuleClass->GetDefaultObject<UModuleBase>()->ModuleName);
}
#endif
