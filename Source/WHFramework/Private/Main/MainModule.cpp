// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/MainModule.h"

#include "Ability/AbilityModule.h"
#include "Common/CommonStatics.h"
#include "Event/EventModuleStatics.h"
#include "Event/Handle/Common/EventHandle_GameExited.h"
#include "Event/Handle/Common/EventHandle_GameInited.h"
#include "Event/Handle/Common/EventHandle_GameStarted.h"
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
}
#endif

void AMainModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();

	for(int32 i = 0; i < Modules.Num(); i++)
	{
		if(Modules[i])
		{
			Modules[i]->OnInitialize();
		}
	}

	UEventModuleStatics::BroadcastEvent<UEventHandle_GameInited>(this);
}

void AMainModule::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);

	for(int32 i = 0; i < Modules.Num(); i++)
	{
		if(Modules[i])
		{
			if(InPhase != EPhase::Final)
			{
				Modules[i]->OnPreparatory(InPhase);
			}
			else if(Modules[i]->IsAutoRunModule())
			{
				Modules[i]->Run();
			}
		}
	}

	if(PHASEC(InPhase, EPhase::Final))
	{
		UEventModuleStatics::BroadcastEvent<UEventHandle_GameStarted>(this);
	}
}

void AMainModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);

	for(int32 i = 0; i < Modules.Num(); i++)
	{
		if(Modules[i] && Modules[i]->GetModuleState() == EModuleState::Running)
		{
			Modules[i]->OnRefresh(DeltaSeconds);
		}
	}
}

void AMainModule::OnTermination_Implementation(EPhase InPhase)
{
	Super::OnTermination_Implementation(InPhase);
	
	for(int32 i = 0; i < Modules.Num(); i++)
	{
		if(Modules[i])
		{
			if(InPhase != EPhase::Final)
			{
				Modules[i]->OnTermination(InPhase);
			}
			else
			{
				Modules[i]->Termination();
			}
		}
	}
	
	if(PHASEC(InPhase, EPhase::Final))
	{
		UEventModuleStatics::BroadcastEvent<UEventHandle_GameExited>(this);

		ModuleMap.Empty();
	}
}

void AMainModule::BeginPlay()
{
	Super::BeginPlay();

	Execute_OnPreparatory(this, EPhase::Primary);
	Execute_OnPreparatory(this, EPhase::Lesser);
	Execute_OnPreparatory(this, EPhase::Final);
}

void AMainModule::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	Execute_OnTermination(this, EPhase::Primary);
	Execute_OnTermination(this, EPhase::Lesser);
	Execute_OnTermination(this, EPhase::Final);
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
	for(int32 i = 0; i < Modules.Num(); i++)
	{
		if(Modules[i])
		{
			Modules[i]->OnDestroy();
		}
	}
	Modules.Empty();
	ModuleMap.Empty();
	Modify();
}
#endif

void AMainModule::PauseModules_Implementation()
{
	for(int32 i = 0; i < Modules.Num(); i++)
	{
		if(Modules[i])
		{
			Modules[i]->Pause();
		}
	}
}

void AMainModule::UnPauseModules_Implementation()
{
	for(int32 i = 0; i < Modules.Num(); i++)
	{
		if(Modules[i])
		{
			Modules[i]->UnPause();
		}
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
	for (int32 i = 0; i < Modules.Num(); i++)
	{
		auto Item = MakeShared<FModuleListItem>();
		if(Modules[i]->GenerateListItem(Item, InFilterText))
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

bool AMainModule::CanAddModule(TSubclassOf<UModuleBase> InModuleClass)
{
	return !ModuleMap.Contains(InModuleClass->GetDefaultObject<UModuleBase>()->ModuleName);
}
#endif
