// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/MainModule.h"

#include "Ability/AbilityModule.h"
#include "Common/CommonStatics.h"
#include "Net/UnrealNetwork.h"

IMPLEMENTATION_MAIN_MODULE(AMainModule)

// ParamSets default values
AMainModule::AMainModule()
{
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
		if(Modules[i] && Modules[i]->IsValidLowLevel())
		{
			Modules[i]->OnInitialize();
		}
	}
}

void AMainModule::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);

	for(int32 i = 0; i < Modules.Num(); i++)
	{
		if(Modules[i] && Modules[i]->IsValidLowLevel())
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
}

void AMainModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);

	for(int32 i = 0; i < Modules.Num(); i++)
	{
		if(Modules[i] && Modules[i]->IsValidLowLevel() && Modules[i]->GetModuleState() == EModuleState::Running)
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
		if(Modules[i] && Modules[i]->IsValidLowLevel())
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
		ModuleMap.Empty();
	}
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
		if(Modules[i] && Modules[i]->IsValidLowLevel())
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
		if(Modules[i] && Modules[i]->IsValidLowLevel())
		{
			Modules[i]->Pause();
		}
	}
}

void AMainModule::UnPauseModules_Implementation()
{
	for(int32 i = 0; i < Modules.Num(); i++)
	{
		if(Modules[i] && Modules[i]->IsValidLowLevel())
		{
			Modules[i]->UnPause();
		}
	}
}

UModuleNetworkComponentBase* AMainModule::GetModuleNetworkComponent(TSubclassOf<UModuleNetworkComponentBase> InClass)
{
	if(Get(false) && Get(false)->IsValidLowLevel())
	{
		if(const AWHPlayerController* PlayerController = UCommonStatics::GetPlayerController<AWHPlayerController>())
		{
			return Cast<UModuleNetworkComponentBase>(PlayerController->GetComponentByClass(InClass));
		}
	}
	return nullptr;
}

void AMainModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMainModule, Modules);
}

#if WITH_EDITOR
void AMainModule::GenerateListItem(TArray<TSharedPtr<FModuleListItem>>& OutModuleListItems)
{
	OutModuleListItems = TArray<TSharedPtr<FModuleListItem>>();
	for (int32 i = 0; i < Modules.Num(); i++)
	{
		auto Item = MakeShared<FModuleListItem>();
		Modules[i]->GenerateListItem(Item);
		OutModuleListItems.Add(Item);
	}
}

void AMainModule::UpdateListItem(TArray<TSharedPtr<FModuleListItem>>& OutModuleListItems)
{
	for (int32 i = 0; i < Modules.Num(); i++)
	{
		Modules[i]->UpdateListItem(OutModuleListItems[i]);
	}
}
#endif
