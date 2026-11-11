// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/MainModule.h"
#include "Common/LifecycleTypes.h"

#include "Ability/AbilityModule.h"
#include "Ability/Inventory/InventoryModule.h"
#include "Common/CommonModuleStatics.h"
#include "Event/EventModuleStatics.h"
#include "Event/Events/Common/Game/Event_GameExited.h"
#include "Event/Events/Common/Game/Event_GameInited.h"
#include "Event/Events/Common/Game/Event_GameStarted.h"
#include "Gameplay/WHGameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Main/WorldNetworkRoute.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

IMPLEMENTATION_MAIN_MODULE(AMainModule)

// ParamSets default values
AMainModule::AMainModule()
{
	// 全局 Tick 同时覆盖编辑器与游戏；禁止 Actor Tick 再次刷新整个模块列表。
	PrimaryActorTick.bCanEverTick = false;
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif

	Modules = TArray<UModuleBase*>();
	ModuleMap = TMap<FName, UModuleBase*>();
	InventoryModuleClass = UInventoryModule::StaticClass();
}

AMainModule::~AMainModule()
{
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
	if (AMainModule::GetPtr(GetWorld()) != this)
	{
		UE_LOG(LogTemp, Error, TEXT("World %s does not have exactly one main module host; initialization refused."), *GetNameSafe(GetWorld()));
		return;
	}
	if (bInitialized || bModulesTerminated || !ValidateAndOrderModules())
	{
		return;
	}
	Super::OnInitialize_Implementation();
	EnsureWorldNetworkRoute();
	SetActorTickEnabled(false);
	LastControlRefreshTime = FPlatformTime::Seconds();

	for(auto Iter : Modules)
	{
		Iter->OnInitialize();
	}

	UEventModuleStatics::BroadcastEvent<FEventGameInited>(this);
}

void AMainModule::EnsureWorldNetworkRoute()
{
	UWorld* World = GetWorld();
	if (WorldRouteAuthority.IsValid() || bModulesTerminated || !World || World->bIsTearingDown ||
		!World->IsGameWorld() || World->GetNetMode() == NM_Client || !World->GetAuthGameMode()) return;
	for (TActorIterator<AWorldNetworkRouteAuthority> It(World); It; ++It) return;
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	WorldRouteAuthority = World->SpawnActor<AWorldNetworkRouteAuthority>(Spawn);
	if (!WorldRouteAuthority.IsValid()) UE_LOG(LogTemp, Error, TEXT("无法建立世界网络纪元，跨网络的世界操作将被拒绝。"));
}

bool AMainModule::ValidateAndOrderModules()
{
	if (!bModuleConfigurationValid)
	{
		return false;
	}
	TMap<FName, UModuleBase*> Candidates;
	for (UModuleBase* Module : Modules)
	{
		if (!::IsValid(Module) || Module->GetOuter() != this || Module->GetModuleScope() != EModuleScope::World || Module->GetModuleName().IsNone())
		{
			UE_LOG(LogTemp, Error, TEXT("Invalid World module on %s"), *GetName());
			return false;
		}
		if (Candidates.Contains(Module->GetModuleName()))
		{
			UE_LOG(LogTemp, Error, TEXT("Duplicate World module identity: %s"), *Module->GetModuleName().ToString());
			return false;
		}
		Candidates.Add(Module->GetModuleName(), Module);
	}
	const UWHGameInstance* GameInstance = GetWorld() ? Cast<UWHGameInstance>(GetWorld()->GetGameInstance()) : nullptr;
	TArray<UModuleBase*> Ordered;
	TSet<FName> OrderedNames;
	while (Ordered.Num() < Modules.Num())
	{
		bool bAdvanced = false;
		for (UModuleBase* Module : Modules)
		{
			if (OrderedNames.Contains(Module->GetModuleName())) continue;
			bool bReady = true;
			for (const FName Dependency : Module->GetModuleDependencies())
			{
				if (Candidates.Contains(Dependency))
				{
					bReady &= OrderedNames.Contains(Dependency);
				}
				else if (!GameInstance || !GameInstance->FindModuleByName(Dependency))
				{
					UE_LOG(LogTemp, Error, TEXT("World module %s has missing dependency %s"), *Module->GetModuleName().ToString(), *Dependency.ToString());
					return false;
				}
			}
			if (bReady)
			{
				Ordered.Add(Module);
				OrderedNames.Add(Module->GetModuleName());
				bAdvanced = true;
			}
		}
		if (!bAdvanced)
		{
			UE_LOG(LogTemp, Error, TEXT("World module dependencies contain a cycle on %s"), *GetName());
			return false;
		}
	}
	Modules = MoveTemp(Ordered);
	ModuleMap = MoveTemp(Candidates);
	RefreshModuleIndices();
	return true;
}

bool AMainModule::RegisterModule(UModuleBase* InModule, int32 InIndex)
{
	if (bInitialized || bModulesTerminated || !::IsValid(InModule) || InModule->GetOuter() != this ||
		InModule->GetModuleScope() != EModuleScope::World || InModule->GetModuleName().IsNone() ||
		ModuleMap.Contains(InModule->GetModuleName()) || Modules.Contains(InModule) ||
		Modules.ContainsByPredicate([InModule](const UModuleBase* Existing) { return Existing && Existing->GetModuleName() == InModule->GetModuleName(); }) ||
		(InIndex != INDEX_NONE && (InIndex < 0 || InIndex > Modules.Num())))
	{
		return false;
	}
#if WITH_EDITOR
	Modify();
#endif
	Modules.Insert(InModule, InIndex == INDEX_NONE ? Modules.Num() : InIndex);
	ModuleMap.Add(InModule->GetModuleName(), InModule);
	RefreshModuleIndices();
	return true;
}

bool AMainModule::ReplaceModule(FName InName, UModuleBase* InModule)
{
	UModuleBase* const* Existing = ModuleMap.Find(InName);
	if (bInitialized || bModulesTerminated || !Existing || !::IsValid(*Existing) || !::IsValid(InModule) ||
		InModule->GetOuter() != this || InModule->GetModuleScope() != EModuleScope::World ||
		InModule->GetModuleName() != InName || !InModule->IsA((*Existing)->GetClass()) || Modules.Contains(InModule))
	{
		return false;
	}
	const int32 Index = Modules.IndexOfByKey(*Existing);
	if (Index == INDEX_NONE) return false;
#if WITH_EDITOR
	Modify();
#endif
	Modules[Index] = InModule;
	ModuleMap[InName] = InModule;
	RefreshModuleIndices();
	return true;
}

bool AMainModule::UnregisterModule(FName InName)
{
	UModuleBase* const* Existing = ModuleMap.Find(InName);
	if (bInitialized || bModulesTerminated || !Existing) return false;
	const int32 Index = Modules.IndexOfByKey(*Existing);
	if (Index == INDEX_NONE) return false;
#if WITH_EDITOR
	Modify();
#endif
	Modules.RemoveAt(Index);
	ModuleMap.Remove(InName);
	RefreshModuleIndices();
	return true;
}

bool AMainModule::MoveModule(FName InName, int32 InIndex)
{
	UModuleBase* const* Existing = ModuleMap.Find(InName);
	if (bInitialized || bModulesTerminated || !Existing || !Modules.IsValidIndex(InIndex)) return false;
	const int32 OldIndex = Modules.IndexOfByKey(*Existing);
	if (OldIndex == INDEX_NONE) return false;
	if (OldIndex == InIndex) return true;
#if WITH_EDITOR
	Modify();
#endif
	UModuleBase* Module = Modules[OldIndex];
	Modules.RemoveAt(OldIndex);
	Modules.Insert(Module, InIndex);
	RefreshModuleIndices();
	return true;
}

void AMainModule::RefreshModuleIndices()
{
	for (int32 Index = 0; Index < Modules.Num(); ++Index)
	{
		if (Modules[Index]) Modules[Index]->ModuleIndex = Index;
	}
}

void AMainModule::OnPreparatory_Implementation()
{
	if (!bInitialized || bModulesPrepared || bModulesTerminated)
	{
		return;
	}
	bModulesPrepared = true;
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

	UEventModuleStatics::BroadcastEvent<FEventGameStarted>(this);
}

void AMainModule::OnRefresh_Implementation(float DeltaSeconds)
{
	EnsureWorldNetworkRoute();
	Super::OnRefresh_Implementation(DeltaSeconds);
	const double Now = FPlatformTime::Seconds();
	const float RealDeltaSeconds = LastControlRefreshTime > 0.0 ? static_cast<float>(Now - LastControlRefreshTime) : 0.0f;
	LastControlRefreshTime = Now;
	const bool bEditor = IsInEditor();
	const bool bWorldPaused = GetWorld() && GetWorld()->IsPaused();

	for(auto Iter : Modules)
	{
		if (!bEditor && bInitialized && !bModulesTerminated && Iter->GetModuleState() != EModuleState::Terminated)
		{
			Iter->OnControlRefresh(RealDeltaSeconds);
		}
		if(bEditor || (bInitialized && !bModulesTerminated && !bWorldPaused && Iter->GetModuleState() == EModuleState::Running))
		{
			Iter->OnRefresh(DeltaSeconds, bEditor);
		}
	}
}

void AMainModule::OnTermination_Implementation()
{
	if (!bInitialized || bModulesTerminated)
	{
		return;
	}
	bModulesTerminated = true;
	TGuardValue<bool> TerminationGuard(bTerminationInProgress, true);
	for (UModuleBase* Module : Modules)
	{
		if (Module) Module->BeginHostTermination();
	}
	for (int32 Index = Modules.Num() - 1; Index >= 0; --Index)
	{
		if (Modules[Index] && Modules[Index]->GetModuleState() != EModuleState::Terminated) Modules[Index]->PrepareForHostTermination();
	}
	const double TerminationStartTime = FPlatformTime::Seconds();
	UE_LOG(LogTemp, Display, TEXT("Main module termination started."));

	Super::OnTermination_Implementation();
		
	ITER_PHASE(Phase,
		for(int32 Index = Modules.Num() - 1; Index >= 0; --Index)
		{
			UModuleBase* Iter = Modules[Index];
			const double ModuleStartTime = FPlatformTime::Seconds();
			Iter->TerminatePhaseFromHost(Phase);
			const double ModuleDuration = FPlatformTime::Seconds() - ModuleStartTime;
			if(ModuleDuration >= 0.1)
			{
				UE_LOG(LogTemp,
				       Warning,
				       TEXT("Slow module termination: %s phase=%d duration=%.3fs"),
				       *Iter->GetModuleName().ToString(),
				       static_cast<int32>(Phase),
				       ModuleDuration);
			}
		}
	)
	
	UEventModuleStatics::BroadcastEvent<FEventGameExited>(this, { true });
	if (WorldRouteAuthority.IsValid()) WorldRouteAuthority->Destroy();
	WorldRouteAuthority.Reset();

	UE_LOG(LogTemp, Display, TEXT("Main module termination callbacks returned in %.3fs."), FPlatformTime::Seconds() - TerminationStartTime);
}

void AMainModule::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (GetWorld() && GetWorld()->IsGameWorld() && InventoryModuleClass)
	{
		const FName Identity = InventoryModuleClass.GetDefaultObject()->GetModuleName();
		if (Identity != GetDefault<UInventoryModule>()->GetModuleName())
		{
			bModuleConfigurationValid = false;
			UE_LOG(LogTemp, Error, TEXT("Inventory module class has an incompatible identity on %s"), *GetName());
			return;
		}
		UModuleBase* const* Existing = ModuleMap.Find(Identity);
		if (Existing && (!*Existing || !(*Existing)->IsA(InventoryModuleClass)))
		{
			bModuleConfigurationValid = false;
			UE_LOG(LogTemp, Error, TEXT("Incompatible inventory module identity on %s"), *GetName());
			return;
		}
		if (!Existing)
		{
			UInventoryModule* Module = NewObject<UInventoryModule>(this, InventoryModuleClass);
			Modules.Insert(Module, 0);
			ModuleMap.Add(Identity, Module);
		}
	}
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
	Modify();
	ModuleMap.Empty();
	for(auto Iter : Modules)
	{
		Iter->Modify();
		Iter->OnGenerate();
		ModuleMap.Add(Iter->GetModuleName(), Iter);
	}
}

void AMainModule::DestroyModules()
{
	Modify();
	for(auto Iter : Modules)
	{
		Iter->Modify();
		Iter->OnDestroy();
	}
	Modules.Empty();
	ModuleMap.Empty();
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

UModuleNetworkComponentBase* AMainModule::GetModuleNetworkComponent(APlayerController* InController, TSubclassOf<UModuleNetworkComponentBase> InClass)
{
	if (IsValid(InController) && InClass && InController->GetWorld() && !InController->GetWorld()->bIsTearingDown &&
		(InController->HasAuthority() || InController->IsLocalController()))
	{
		return Cast<UModuleNetworkComponentBase>(InController->GetComponentByClass(InClass));
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
	const UModuleBase* DefaultModule = InModuleClass ? InModuleClass.GetDefaultObject() : nullptr;
	return DefaultModule && DefaultModule->GetModuleScope() == EModuleScope::World && !ModuleMap.Contains(DefaultModule->GetModuleName());
}
#endif
