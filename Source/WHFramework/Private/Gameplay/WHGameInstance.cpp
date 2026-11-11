// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Gameplay/WHGameInstance.h"
#include "Common/LifecycleTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Main/Base/ModuleBase.h"
#include "Ability/Inventory/InventoryClientReceiptLedger.h"
#include "Engine/LocalPlayer.h"
#include "Network/NetworkModule.h"
#include "SaveGame/SaveGameModule.h"
#include "Widget/Theme/WidgetThemeModule.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/UObjectGlobals.h"

UWHGameInstance::UWHGameInstance()
{
	SetModuleTemplate(CreateDefaultSubobject<UWidgetThemeModule>(TEXT("WidgetThemeModuleTemplate")));
	SetModuleTemplate(CreateDefaultSubobject<UNetworkModule>(TEXT("NetworkModuleTemplate")));
}

TSharedPtr<FInventoryClientReceiptLedger, ESPMode::ThreadSafe> UWHGameInstance::GetInventoryReceiptLedger(ULocalPlayer* InPlayer)
{
	if (!::IsValid(InPlayer) || InPlayer->GetGameInstance() != this || bModulesTerminated) return nullptr;
	for (auto It = InventoryReceiptLedgers.CreateIterator(); It; ++It)
		if (!It.Key().IsValid()) It.RemoveCurrent();
	TSharedPtr<FInventoryClientReceiptLedger, ESPMode::ThreadSafe>& Ledger = InventoryReceiptLedgers.FindOrAdd(InPlayer);
	if (!Ledger) Ledger = MakeShared<FInventoryClientReceiptLedger, ESPMode::ThreadSafe>();
	return Ledger;
}

void UWHGameInstance::SetModuleTemplate(UModuleBase* InTemplate)
{
	check(InTemplate);
	check(InTemplate->GetModuleScope() == EModuleScope::GameInstance);
	for (TObjectPtr<UModuleBase>& Existing : ModuleTemplates)
	{
		if (Existing && Existing->GetModuleName() == InTemplate->GetModuleName())
		{
			checkf(InTemplate->IsA(Existing->GetClass()), TEXT("Incompatible replacement for GameInstance module %s"), *InTemplate->GetModuleName().ToString());
			Existing = InTemplate;
			return;
		}
	}
	ModuleTemplates.Add(InTemplate);
}

void UWHGameInstance::Init()
{
	Super::Init();
	FParse::Value(FCommandLine::Get(), TEXT("LocalDataId="), RequestedLocalDataNamespace);
	if (GEngine)
	{
		SaveLoadTravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::OnSaveLoadTravelFailure);
	}
	if (InitializeModules())
	{
		ModuleTickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickModules));
	}
}

bool UWHGameInstance::InitializeModules()
{
	TMap<FName, UModuleBase*> TemplatesByName;
	for (UModuleBase* Template : ModuleTemplates)
	{
		if (!Template || Template->GetModuleScope() != EModuleScope::GameInstance || Template->GetModuleName().IsNone())
		{
			UE_LOG(LogTemp, Error, TEXT("Invalid GameInstance module template on %s"), *GetName());
			return false;
		}
		if (!Template->ShouldCreateForGameInstance(this))
		{
			continue;
		}
		if (TemplatesByName.Contains(Template->GetModuleName()))
		{
			UE_LOG(LogTemp, Error, TEXT("Duplicate GameInstance module identity: %s"), *Template->GetModuleName().ToString());
			return false;
		}
		TemplatesByName.Add(Template->GetModuleName(), Template);
	}

	TArray<UModuleBase*> OrderedTemplates;
	TSet<FName> OrderedNames;
	while (OrderedTemplates.Num() < TemplatesByName.Num())
	{
		bool bAdvanced = false;
		for (UModuleBase* Template : ModuleTemplates)
		{
			if (!Template || !TemplatesByName.Contains(Template->GetModuleName()) || OrderedNames.Contains(Template->GetModuleName()))
			{
				continue;
			}
			bool bReady = true;
			for (const FName Dependency : Template->GetModuleDependencies())
			{
				if (!TemplatesByName.Contains(Dependency))
				{
					UE_LOG(LogTemp, Error, TEXT("GameInstance module %s has missing dependency %s"), *Template->GetModuleName().ToString(), *Dependency.ToString());
					return false;
				}
				bReady &= OrderedNames.Contains(Dependency);
			}
			if (bReady)
			{
				OrderedTemplates.Add(Template);
				OrderedNames.Add(Template->GetModuleName());
				bAdvanced = true;
			}
		}
		if (!bAdvanced)
		{
			UE_LOG(LogTemp, Error, TEXT("GameInstance module dependencies contain a cycle"));
			return false;
		}
	}

	for (UModuleBase* Template : OrderedTemplates)
	{
		const FName RuntimeName = MakeUniqueObjectName(this, Template->GetClass(), FName(*(Template->GetName() + TEXT("_Runtime"))));
		UModuleBase* Module = DuplicateObject<UModuleBase>(Template, this, RuntimeName);
		check(Module);
		Modules.Add(Module);
	}
	for (UModuleBase* Module : Modules)
	{
		Module->OnInitialize();
	}
	ITER_PHASE(Phase,
		for (UModuleBase* Module : Modules)
		{
			if (Phase != EPhase::Final)
			{
				Module->OnPreparatory(Phase);
			}
			else if (Module->IsModuleAutoRun())
			{
				Module->Run();
			}
		}
	)
	return true;
}

UModuleBase* UWHGameInstance::FindModuleByClass(TSubclassOf<UModuleBase> InClass) const
{
	if (!InClass)
	{
		return nullptr;
	}
	const FName Identity = InClass.GetDefaultObject()->GetModuleName();
	UModuleBase* Module = FindModuleByName(Identity);
	return Module && Module->IsA(InClass) ? Module : nullptr;
}

#if WITH_EDITOR
UModuleBase* UWHGameInstance::FindModuleTemplateForEditor(FName InName) const
{
	UModuleBase* Found = nullptr;
	for (UModuleBase* Template : ModuleTemplates)
	{
		if (Template && Template->GetModuleName() == InName)
		{
			if (Found) return nullptr;
			Found = Template;
		}
	}
	return Found;
}
#endif

UModuleBase* UWHGameInstance::FindModuleByName(FName InName) const
{
	for (UModuleBase* Module : Modules)
	{
		if (::IsValid(Module) && Module->IsBusinessAdmissionOpen() && Module->GetModuleName() == InName)
		{
			return Module;
		}
	}
	return nullptr;
}

bool UWHGameInstance::TickModules(float DeltaSeconds)
{
	if (bModulesTerminated) return false;
	const TArray<TObjectPtr<UModuleBase>> ModulesToRefresh = Modules;
	for (UModuleBase* Module : ModulesToRefresh)
	{
		if (!Module || !Module->IsBusinessAdmissionOpen()) continue;
		if (Module->GetModuleState() != EModuleState::Terminated)
		{
			Module->OnControlRefresh(DeltaSeconds);
		}
		const UWorld* World = GetWorld();
		if (World && !World->IsPaused() && Module->GetModuleState() == EModuleState::Running)
		{
			Module->OnRefresh(DeltaSeconds, false);
		}
	}
	return true;
}

void UWHGameInstance::TerminateModules()
{
	if (bModulesTerminated) return;
	bModulesTerminated = true;
	FTSTicker::GetCoreTicker().RemoveTicker(ModuleTickHandle);
	ModuleTickHandle.Reset();
	for (UModuleBase* Module : Modules)
	{
		if (Module) Module->BeginHostTermination();
	}
	for (int32 Index = Modules.Num() - 1; Index >= 0; --Index)
	{
		if (Modules[Index] && Modules[Index]->GetModuleState() != EModuleState::Terminated) Modules[Index]->PrepareForHostTermination();
	}
	for (const EPhase Phase : { EPhase::Primary, EPhase::Lesser, EPhase::Final })
	{
		for (int32 Index = Modules.Num() - 1; Index >= 0; --Index)
		{
			if (Modules[Index]) Modules[Index]->TerminatePhaseFromHost(Phase);
		}
	}
	Modules.Reset();
}

bool UWHGameInstance::EnsureLocalSaveContext()
{
	if (LocalSaveContext) return true;
	auto Context = MakeShared<FSaveGameLocalContext, ESPMode::ThreadSafe>();
	if (Context->Acquire(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames")), RequestedLocalDataNamespace, LocalSaveContextError))
	{
		LocalSaveContext = MoveTemp(Context);
		UE_LOG(LogTemp, Display, TEXT("Local save instance acquired: namespace=%s root=%s"),
			*LocalSaveContext->GetNamespace(), *LocalSaveContext->GetStorageRoot());
		return true;
	}
	UE_LOG(LogTemp, Error, TEXT("Local save instance rejected: %s"), *LocalSaveContextError);
	return false;
}

void UWHGameInstance::Shutdown()
{
	TerminateModules();
	if (GEngine)
	{
		GEngine->OnTravelFailure().Remove(SaveLoadTravelFailureHandle);
	}
	PendingSaveLoad = FSaveGameLoadSnapshot();
	PendingLoadSource.Reset();
	Super::Shutdown();
	LocalSaveContext.Reset();
}

bool UWHGameInstance::SetPendingSaveLoad(FSaveGameLoadSnapshot InSnapshot, USaveGameModule* InSource)
{
	if (!InSnapshot.CanApply(InSnapshot.Context.LoadPhase) || InSnapshot.AppliedPhases != EPhase::None || PendingSaveLoad.IsValid())
	{
		return false;
	}
	PendingSaveLoad = MoveTemp(InSnapshot);
	PendingLoadSource = InSource;
	return true;
}

void UWHGameInstance::OnSaveLoadTravelFailure(UWorld* InWorld, ETravelFailure::Type InType, const FString& InError)
{
	USaveGameModule* Source = PendingLoadSource.Get();
	if (!PendingSaveLoad.IsValid() || !Source || Source->GetWorld() != InWorld)
	{
		return;
	}
	const FGuid OperationId = PendingSaveLoad.Context.OperationId;
	Source->CancelLoadOperation(OperationId);
	CancelPendingSaveLoad(OperationId);
	UE_LOG(LogTemp, Error, TEXT("存档切图失败：操作=%s，原因=%s"), *OperationId.ToString(), *InError);
}
