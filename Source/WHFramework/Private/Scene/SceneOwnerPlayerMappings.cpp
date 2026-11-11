#include "Scene/SceneModule.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Event/Events/Scene/Event_SetDataLayerOwnerPlayer.h"
#include "Event/Events/Scene/Event_SetDataLayerRuntimeState.h"
#include "Event/Events/Scene/Event_SetLevelOwnerPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Misc/PackageName.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"

void USceneModule::BeginOwnerMappingMonitoring()
{
	UWorld* World = GetWorld();
	if (!::IsValid(World) || World->bIsTearingDown || OwnerMappingWorld.IsValid()) return;
	OwnerMappingWorld = World;
	OwnerMappingSpawnHandle = World->AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateUObject(this, &ThisClass::OnOwnerMappingActorSpawned));
	OwnerMappingLevelAddedHandle = FWorldDelegates::LevelAddedToWorld.AddUObject(this, &ThisClass::OnOwnerMappingLevelChanged);
	OwnerMappingLevelRemovedHandle = FWorldDelegates::PreLevelRemovedFromWorld.AddUObject(this, &ThisClass::OnOwnerMappingLevelRemoved);
}

void USceneModule::EndOwnerMappingMonitoring()
{
	UWorld* World = OwnerMappingWorld.Get();
	if (World && OwnerMappingSpawnHandle.IsValid()) World->RemoveOnActorSpawnedHandler(OwnerMappingSpawnHandle);
	if (OwnerMappingLevelAddedHandle.IsValid()) FWorldDelegates::LevelAddedToWorld.Remove(OwnerMappingLevelAddedHandle);
	if (OwnerMappingLevelRemovedHandle.IsValid()) FWorldDelegates::PreLevelRemovedFromWorld.Remove(OwnerMappingLevelRemovedHandle);
	OwnerMappingSpawnHandle.Reset();
	OwnerMappingLevelAddedHandle.Reset();
	OwnerMappingLevelRemovedHandle.Reset();
	DataLayerPlayerMappings.Reset();
	LevelPlayerMappings.Reset();
	OwnerMappingViewTargets.Reset();
	PendingOwnerMappingActors.Reset();
	bOwnerMappingFullRefresh = false;
	TArray<TWeakObjectPtr<AActor>> Actors;
	OwnerMappingOverrides.GetKeys(Actors);
	for (const TWeakObjectPtr<AActor>& Actor : Actors)
	{
		if (Actor.IsValid() && World && !World->bIsTearingDown && Actor->GetWorld() == World)
			ReleaseOwnerMappingActor(Actor.Get(), World);
	}
	OwnerMappingOverrides.Reset();
	OwnerMappingWorld.Reset();
}

void USceneModule::OnOwnerMappingActorSpawned(AActor* Actor)
{
	if (IsBusinessAdmissionOpen() && !bSceneRuntimeClosing && ::IsValid(Actor) && Actor->GetWorld() == OwnerMappingWorld.Get() &&
		(!DataLayerPlayerMappings.IsEmpty() || !LevelPlayerMappings.IsEmpty()))
		PendingOwnerMappingActors.Add(Actor);
}

void USceneModule::OnOwnerMappingLevelChanged(ULevel* Level, UWorld* World)
{
	if (!IsBusinessAdmissionOpen() || bSceneRuntimeClosing || !Level || World != OwnerMappingWorld.Get() ||
		(DataLayerPlayerMappings.IsEmpty() && LevelPlayerMappings.IsEmpty() && OwnerMappingOverrides.IsEmpty())) return;
	for (AActor* Actor : Level->Actors) if (::IsValid(Actor)) PendingOwnerMappingActors.Add(Actor);
}

void USceneModule::OnOwnerMappingLevelRemoved(ULevel* Level, UWorld* World)
{
	if (bSceneRuntimeClosing || !::IsValid(World) || World != OwnerMappingWorld.Get()) return;
	RequestOwnerMappingRefresh();
	TArray<TWeakObjectPtr<AActor>> Removed;
	for (const auto& Pair : OwnerMappingOverrides)
	{
		AActor* Actor = Pair.Key.Get();
		if (Actor && (!Level || Actor->GetLevel() == Level)) Removed.Add(Actor);
	}
	TGuardValue<bool> Reconciling(bReconcilingOwnerMappings, true);
	for (const TWeakObjectPtr<AActor>& Actor : Removed)
	{
		PendingOwnerMappingActors.Remove(Actor);
		if (Actor.IsValid()) ReleaseOwnerMappingActor(Actor.Get(), World);
	}
}

void USceneModule::RequestOwnerMappingRefresh()
{
	bOwnerMappingFullRefresh = true;
}

void USceneModule::ApplyAllOwnerPlayerMappings()
{
	if (!IsBusinessAdmissionOpen() || bSceneRuntimeClosing) return;
	RequestOwnerMappingRefresh();
	ReconcileOwnerPlayerMappings();
}

void USceneModule::OnSetDataLayerRuntimeState(UObject* InSender, const FEventSetDataLayerRuntimeState& InEvent)
{
	UWorld* World = OwnerMappingWorld.Get();
	if (!IsBusinessAdmissionOpen() || bSceneRuntimeClosing || !World || !InEvent.DataLayer) return;
	if (UDataLayerManager* Manager = UDataLayerManager::GetDataLayerManager(World))
	{
		Manager->SetDataLayerRuntimeState(InEvent.DataLayer, InEvent.State, InEvent.bRecursive);
		ApplyAllOwnerPlayerMappings();
	}
}

void USceneModule::OnSetDataLayerOwnerPlayer(UObject* InSender, const FEventSetDataLayerOwnerPlayer& InEvent)
{
	UWorld* World = OwnerMappingWorld.Get();
	if (!IsBusinessAdmissionOpen() || bSceneRuntimeClosing || !World || !InEvent.DataLayer ||
		(InEvent.Player && (!::IsValid(InEvent.Player) || InEvent.Player->GetWorld() != World))) return;
	if (InEvent.Player) DataLayerPlayerMappings.Add(InEvent.DataLayer, InEvent.Player);
	else DataLayerPlayerMappings.Remove(InEvent.DataLayer);
	ApplyAllOwnerPlayerMappings();
}

void USceneModule::OnSetLevelOwnerPlayer(UObject* InSender, const FEventSetLevelOwnerPlayer& InEvent)
{
	UWorld* World = OwnerMappingWorld.Get();
	if (!IsBusinessAdmissionOpen() || bSceneRuntimeClosing || !World ||
		(InEvent.Player && (!::IsValid(InEvent.Player) || InEvent.Player->GetWorld() != World))) return;
	const FString RawPath = InEvent.LevelObjectPtr.IsNull() ? InEvent.LevelPath.ToString() : InEvent.LevelObjectPtr.ToString();
	if (RawPath.IsEmpty() || RawPath == TEXT("None")) return;
	FString Package = UWorld::RemovePIEPrefix(FPackageName::ObjectPathToPackageName(RawPath));
	if (FPackageName::IsShortPackageName(Package))
	{
		FString Match;
		bool bAmbiguous = false;
		auto Consider = [&](const FString& Candidate)
		{
			const FString Normalized = UWorld::RemovePIEPrefix(Candidate);
			if (!FPackageName::GetShortName(Normalized).Equals(Package, ESearchCase::IgnoreCase)) return;
			if (!Match.IsEmpty() && !Match.Equals(Normalized, ESearchCase::IgnoreCase)) bAmbiguous = true;
			else Match = Normalized;
		};
		if (World->PersistentLevel) Consider(World->PersistentLevel->GetOutermost()->GetName());
		for (const ULevelStreaming* Level : World->GetStreamingLevels()) if (Level) Consider(Level->GetWorldAssetPackageName());
		if (Match.IsEmpty() || bAmbiguous)
		{
			UE_LOG(LogTemp, Warning, TEXT("无法唯一确定场景玩家归属的关卡路径：%s"), *RawPath);
			return;
		}
		Package = MoveTemp(Match);
	}
	const FName LevelPath(*Package);
	if (InEvent.Player) LevelPlayerMappings.Add(LevelPath, InEvent.Player);
	else LevelPlayerMappings.Remove(LevelPath);
	ApplyAllOwnerPlayerMappings();
}

void USceneModule::RefreshOwnerPlayerMappings()
{
	UWorld* World = OwnerMappingWorld.Get();
	if (!IsBusinessAdmissionOpen() || !::IsValid(World) || World->bIsTearingDown || World != GetWorld() || bSceneRuntimeClosing) return;
	bool bChanged = false;
	for (auto It = DataLayerPlayerMappings.CreateIterator(); It; ++It)
	{
		APlayerController* Player = It.Value().Get();
		if (!::IsValid(It.Key()) || !Player || Player->GetWorld() != World || Player->IsActorBeingDestroyed())
		{
			It.RemoveCurrent();
			bChanged = true;
		}
	}
	for (auto It = LevelPlayerMappings.CreateIterator(); It; ++It)
	{
		APlayerController* Player = It.Value().Get();
		if (!Player || Player->GetWorld() != World || Player->IsActorBeingDestroyed())
		{
			It.RemoveCurrent();
			bChanged = true;
		}
	}
	TMap<TWeakObjectPtr<APlayerController>, TWeakObjectPtr<AActor>> CurrentTargets;
	auto CaptureTarget = [&CurrentTargets](APlayerController* Player)
	{
		if (Player) CurrentTargets.Add(Player, Player->GetViewTarget());
	};
	for (const auto& Pair : DataLayerPlayerMappings) CaptureTarget(Pair.Value.Get());
	for (const auto& Pair : LevelPlayerMappings) CaptureTarget(Pair.Value.Get());
	if (CurrentTargets.Num() != OwnerMappingViewTargets.Num()) bChanged = true;
	for (const auto& Pair : CurrentTargets)
	{
		const TWeakObjectPtr<AActor>* OldTarget = OwnerMappingViewTargets.Find(Pair.Key);
		if (!OldTarget || *OldTarget != Pair.Value) bChanged = true;
	}
	OwnerMappingViewTargets = MoveTemp(CurrentTargets);
	for (auto It = OwnerMappingOverrides.CreateIterator(); It; ++It)
	{
		AActor* Actor = It.Key().Get();
		if (!Actor || Actor->GetWorld() != World || Actor->IsActorBeingDestroyed())
		{
			It.RemoveCurrent();
			continue;
		}
		const FSceneOwnerActorOverride& State = It.Value();
		bool bNeedsUpdate = State.bOwnerRetry || Actor->GetOwner() != State.LastOwner.Get();
		TArray<UPrimitiveComponent*> Components;
		Actor->GetComponents<UPrimitiveComponent>(Components);
		for (UPrimitiveComponent* Component : Components)
		{
			const FSceneOwnerComponentOverride* ComponentState = State.Components.Find(Component);
			if (::IsValid(Component) && (!ComponentState || !Component->bOnlyOwnerSee ||
				(ComponentState->bVisibilityManaged && Component->GetVisibleFlag() != ComponentState->bLastVisible))) bNeedsUpdate = true;
		}
		if (Components.Num() != State.Components.Num()) bNeedsUpdate = true;
		if (bNeedsUpdate) PendingOwnerMappingActors.Add(Actor);
	}
	if (bChanged) RequestOwnerMappingRefresh();
	ReconcileOwnerPlayerMappings();
}

void USceneModule::ReleaseOwnerMappingComponent(AActor* PreviousOwner, UPrimitiveComponent* Component,
	const FSceneOwnerComponentOverride& State, UWorld* World)
{
	if (!Component) return;
	if (State.bManaged && Component->bOnlyOwnerSee == State.bLastOnlyOwnerSee)
		Component->SetOnlyOwnerSee(State.bOriginalOnlyOwnerSee);
	if (State.bVisibilityManaged && Component->GetVisibleFlag() == State.bLastVisible)
		Component->SetVisibility(State.bOriginalVisible);
}

void USceneModule::ReleaseOwnerMappingActor(AActor* Actor, UWorld* World)
{
	FSceneOwnerActorOverride State;
	if (!Actor || !OwnerMappingOverrides.RemoveAndCopyValue(Actor, State)) return;
	for (const auto& Pair : State.Components)
		ReleaseOwnerMappingComponent(Actor, Pair.Key.Get(), Pair.Value, World);
	if (State.bManaged && Actor->GetOwner() == State.LastOwner.Get())
		Actor->SetOwner(State.OriginalOwner.Get());
}

void USceneModule::ReconcileOwnerPlayerMappings()
{
	UWorld* World = OwnerMappingWorld.Get();
	if (!IsBusinessAdmissionOpen() || bReconcilingOwnerMappings || bSceneRuntimeClosing || !::IsValid(World) ||
		World->bIsTearingDown || World != GetWorld()) return;
	TGuardValue<bool> Reconciling(bReconcilingOwnerMappings, true);
	TSet<TWeakObjectPtr<AActor>> Candidates = MoveTemp(PendingOwnerMappingActors);
	PendingOwnerMappingActors.Reset();
	if (bOwnerMappingFullRefresh)
	{
		bOwnerMappingFullRefresh = false;
		for (TActorIterator<AActor> It(World); It; ++It) Candidates.Add(*It);
		for (const auto& Pair : OwnerMappingOverrides) Candidates.Add(Pair.Key);
	}
	for (const TWeakObjectPtr<AActor>& Candidate : Candidates)
	{
		AActor* Actor = Candidate.Get();
		if (!Actor || Actor->IsActorBeingDestroyed() || Actor->GetWorld() != World)
		{
			OwnerMappingOverrides.Remove(Candidate);
			continue;
		}
		if (const ULevel* Level = Actor->GetLevel(); Level && Level->bIsBeingRemoved)
		{
			ReleaseOwnerMappingActor(Actor, World);
			continue;
		}
		APlayerController* ClaimedPlayer = nullptr;
		bool bHasClaim = false;
		bool bConflict = false;
		auto AddClaim = [&](APlayerController* Player)
		{
			if (!Player || Player->GetWorld() != World || Player->IsActorBeingDestroyed()) return;
			if (!bHasClaim) { ClaimedPlayer = Player; bHasClaim = true; }
			else if (ClaimedPlayer != Player) bConflict = true;
		};
		for (const auto& Pair : DataLayerPlayerMappings)
		{
			if (::IsValid(Pair.Key) && Actor->ContainsDataLayer(Pair.Key)) AddClaim(Pair.Value.Get());
		}
		if (const ULevel* Level = Actor->GetLevel())
		{
			const FName Path(*UWorld::RemovePIEPrefix(Level->GetOutermost()->GetName()));
			if (const TWeakObjectPtr<APlayerController>* Player = LevelPlayerMappings.Find(Path)) AddClaim(Player->Get());
		}
		if (!bHasClaim)
		{
			ReleaseOwnerMappingActor(Actor, World);
			continue;
		}
		AActor* DesiredOwner = !bConflict ? ClaimedPlayer->GetViewTarget() : nullptr;
		if (DesiredOwner && (!::IsValid(DesiredOwner) || DesiredOwner->GetWorld() != World || DesiredOwner->IsActorBeingDestroyed()))
			DesiredOwner = nullptr;
		if (DesiredOwner && (DesiredOwner == Actor || DesiredOwner->IsOwnedBy(Actor)))
			DesiredOwner = ClaimedPlayer != Actor && !ClaimedPlayer->IsOwnedBy(Actor) ? ClaimedPlayer : nullptr;
		FSceneOwnerActorOverride* State = OwnerMappingOverrides.Find(Actor);
		if (!State)
		{
			FSceneOwnerActorOverride Initial;
			Initial.OriginalOwner = Actor->GetOwner();
			Initial.LastOwner = Actor->GetOwner();
			State = &OwnerMappingOverrides.Add(Actor, MoveTemp(Initial));
		}
		if (bConflict && !State->bConflict)
			UE_LOG(LogTemp, Warning, TEXT("场景实体有多个不同玩家归属，暂对所有玩家隐藏：%s"), *GetNameSafe(Actor));
		State->bConflict = bConflict;
		const bool bChangingOwner = Actor->GetOwner() != DesiredOwner;
		// Apply visibility before SetOwner can invoke external callbacks.
		TArray<UPrimitiveComponent*> InitialComponents;
		Actor->GetComponents<UPrimitiveComponent>(InitialComponents);
		TArray<TWeakObjectPtr<UPrimitiveComponent>> InitialComponentRefs;
		for (UPrimitiveComponent* Component : InitialComponents) InitialComponentRefs.Add(Component);
		for (const TWeakObjectPtr<UPrimitiveComponent>& ComponentRef : InitialComponentRefs)
		{
			UPrimitiveComponent* Component = ComponentRef.Get();
			if (!::IsValid(Component) || Component->GetOwner() != Actor) continue;
			FSceneOwnerComponentOverride* ComponentState = State->Components.Find(Component);
			if (!ComponentState)
			{
				FSceneOwnerComponentOverride Initial;
				Initial.bOriginalOnlyOwnerSee = Component->bOnlyOwnerSee;
				Initial.bLastOnlyOwnerSee = Component->bOnlyOwnerSee;
				ComponentState = &State->Components.Add(Component, Initial);
			}
			if (ComponentState->bManaged && Component->bOnlyOwnerSee != ComponentState->bLastOnlyOwnerSee)
			{
				ComponentState->bOriginalOnlyOwnerSee = Component->bOnlyOwnerSee;
				ComponentState->bManaged = false;
			}
			if (!Component->bOnlyOwnerSee)
			{
				ComponentState->bOriginalOnlyOwnerSee = false;
				ComponentState->bLastOnlyOwnerSee = true;
				ComponentState->bManaged = true;
				Component->SetOnlyOwnerSee(true);
				State = OwnerMappingOverrides.Find(Actor);
				if (!State) return;
			}
			ComponentState = State->Components.Find(Component);
			if (!ComponentState) return;
			const bool bVisible = Component->GetVisibleFlag();
			if (ComponentState->bVisibilityManaged && bVisible != ComponentState->bLastVisible)
			{
				ComponentState->bOriginalVisible = bVisible;
				ComponentState->bVisibilityManaged = false;
			}
			if (bChangingOwner && bVisible)
			{
				ComponentState->bOriginalVisible = true;
				ComponentState->bLastVisible = false;
				ComponentState->bVisibilityManaged = true;
				Component->SetVisibility(false);
				State = OwnerMappingOverrides.Find(Actor);
				if (!State) return;
			}
		}
		AActor* CurrentOwner = Actor->GetOwner();
		if (CurrentOwner != State->LastOwner.Get())
		{
			State->OriginalOwner = CurrentOwner;
			State->LastOwner = CurrentOwner;
			State->bManaged = false;
		}
		if (CurrentOwner != DesiredOwner)
		{
			if (!State->bManaged) State->OriginalOwner = CurrentOwner;
			State->LastOwner = DesiredOwner;
			State->bManaged = true;
			Actor->SetOwner(DesiredOwner);
			if (Actor->GetOwner() != DesiredOwner)
			{
				State = OwnerMappingOverrides.Find(Actor);
				if (!State) return;
				// SetOwner 可同步调用外部逻辑；保留回调设置的新 Owner，不再用空值覆盖它。
				State->OriginalOwner = Actor->GetOwner();
				State->LastOwner = Actor->GetOwner();
				State->bManaged = false;
				UE_LOG(LogTemp, Warning, TEXT("场景实体的玩家归属被外部回调改变，暂时隐藏并保留外部 Owner：%s"), *GetNameSafe(Actor));
			}
		}
		State = OwnerMappingOverrides.Find(Actor);
		if (!State) return;
		State->bOwnerRetry = Actor->GetOwner() != DesiredOwner;
		const bool bForceHidden = bConflict || !DesiredOwner || Actor->GetOwner() != DesiredOwner;
		TArray<UPrimitiveComponent*> Components;
		Actor->GetComponents<UPrimitiveComponent>(Components);
		TArray<TWeakObjectPtr<UPrimitiveComponent>> ComponentRefs;
		for (UPrimitiveComponent* Component : Components) ComponentRefs.Add(Component);
		TSet<TWeakObjectPtr<UPrimitiveComponent>> CurrentComponents;
		for (const TWeakObjectPtr<UPrimitiveComponent>& ComponentRef : ComponentRefs)
		{
			UPrimitiveComponent* Component = ComponentRef.Get();
			if (!::IsValid(Component) || Component->GetOwner() != Actor) continue;
			CurrentComponents.Add(Component);
			FSceneOwnerComponentOverride* ComponentState = State->Components.Find(Component);
			if (!ComponentState)
			{
				FSceneOwnerComponentOverride Initial;
				Initial.bOriginalOnlyOwnerSee = Component->bOnlyOwnerSee;
				Initial.bLastOnlyOwnerSee = Component->bOnlyOwnerSee;
				ComponentState = &State->Components.Add(Component, Initial);
			}
			if (ComponentState->bManaged && Component->bOnlyOwnerSee != ComponentState->bLastOnlyOwnerSee)
			{
				ComponentState->bOriginalOnlyOwnerSee = Component->bOnlyOwnerSee;
				ComponentState->bManaged = false;
			}
			if (!Component->bOnlyOwnerSee)
			{
				if (!ComponentState->bManaged) ComponentState->bOriginalOnlyOwnerSee = false;
				ComponentState->bLastOnlyOwnerSee = true;
				ComponentState->bManaged = true;
				Component->SetOnlyOwnerSee(true);
				State = OwnerMappingOverrides.Find(Actor);
				if (!State) return;
			}
			ComponentState = State->Components.Find(Component);
			if (!ComponentState) return;
			const bool bVisible = Component->GetVisibleFlag();
			if (ComponentState->bVisibilityManaged && bVisible != ComponentState->bLastVisible)
			{
				ComponentState->bOriginalVisible = bVisible;
				ComponentState->bVisibilityManaged = false;
			}
			if (bForceHidden && bVisible)
			{
				ComponentState->bOriginalVisible = true;
				ComponentState->bLastVisible = false;
				ComponentState->bVisibilityManaged = true;
				Component->SetVisibility(false);
			}
			else if (!bForceHidden && ComponentState->bVisibilityManaged)
			{
				const bool bOriginal = ComponentState->bOriginalVisible;
				ComponentState->bVisibilityManaged = false;
				Component->SetVisibility(bOriginal);
			}
			State = OwnerMappingOverrides.Find(Actor);
			if (!State) return;
		}
		TArray<TWeakObjectPtr<UPrimitiveComponent>> RemovedComponents;
		for (const auto& Pair : State->Components)
			if (!Pair.Key.IsValid() || !CurrentComponents.Contains(Pair.Key)) RemovedComponents.Add(Pair.Key);
		for (const TWeakObjectPtr<UPrimitiveComponent>& Key : RemovedComponents)
		{
			FSceneOwnerComponentOverride Old;
			if (!State->Components.RemoveAndCopyValue(Key, Old)) continue;
			ReleaseOwnerMappingComponent(Actor, Key.Get(), Old, World);
			State = OwnerMappingOverrides.Find(Actor);
			if (!State) return;
		}
	}
}
