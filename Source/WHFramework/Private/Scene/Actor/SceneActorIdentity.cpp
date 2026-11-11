#include "Scene/Actor/SceneActorIdentity.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Scene/SceneModule.h"
#include "Scene/Actor/SceneActorInterface.h"

void SceneActorIdentity::InitializeCreated(AActor& Actor, FGuid& Identity)
{
	if (Actor.IsTemplate()) return;
	Identity = Actor.HasAuthority() ? FGuid::NewGuid() : FGuid();
}

void SceneActorIdentity::InitializeEditorDuplicate(AActor& Actor, FGuid& Identity, EDuplicateMode::Type DuplicateMode)
{
#if WITH_EDITOR
	const UWorld* World = Actor.GetWorld();
	if (DuplicateMode == EDuplicateMode::Normal && !Actor.IsTemplate() && World && World->WorldType == EWorldType::Editor)
		Identity = FGuid::NewGuid();
#endif
}

void SceneActorIdentity::InitializeEditorImport(AActor& Actor, FGuid& Identity)
{
#if WITH_EDITOR
	const UWorld* World = Actor.GetWorld();
	if (!Actor.IsTemplate() && World && World->WorldType == EWorldType::Editor) Identity = FGuid::NewGuid();
#endif
}

bool SceneActorIdentity::Assign(AActor& Actor, FGuid& Identity, const FGuid& NewIdentity)
{
	if (!NewIdentity.IsValid()) return false;
	UWorld* World = Actor.GetWorld();
	if (!World)
	{
		Identity = NewIdentity;
		return true;
	}
	if (!World->IsGameWorld())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Existing = *It;
			if (Existing == &Actor || !Existing->Implements<USceneActorInterface>()) continue;
			const FGuid ExistingId = ISceneActorInterface::Execute_GetActorID(Existing);
			if (ExistingId == NewIdentity) return false;
		}
		Identity = NewIdentity;
		return true;
	}
	if (!Actor.HasAuthority()) return false;
	USceneModule* Scene = USceneModule::GetPtr(World);
	if (!Scene) return false;
	const FGuid OldIdentity = Identity;
	return Scene->ReassignSceneActorIdentity(&Actor, OldIdentity, NewIdentity, [&Identity, NewIdentity]() { Identity = NewIdentity; });
}

void SceneActorIdentity::QueueReplication(AActor& Actor, FTSTicker::FDelegateHandle& RetryTicker)
{
	UWorld* World = Actor.GetWorld();
	if (!World) return;
	CancelReplication(RetryTicker);
	const FGuid ReceivedIdentity = ISceneActorInterface::Execute_GetActorID(&Actor);
	if (!ReceivedIdentity.IsValid())
	{
		if (USceneModule* Scene = USceneModule::GetPtr(World)) Scene->RemoveSceneActor(&Actor);
		return;
	}
	if (USceneModule* Scene = USceneModule::GetPtr(World))
	{
		if (!Scene->QueueSceneActorRegistration(&Actor, ReceivedIdentity))
			UE_LOG(LogTemp, Error, TEXT("场景实体复制身份未能排队：%s id=%s"), *GetNameSafe(&Actor), *ReceivedIdentity.ToString());
		return;
	}
	const TWeakObjectPtr<AActor> WeakActor(&Actor);
	RetryTicker = FTSTicker::GetCoreTicker().AddTicker(TEXT("SceneActorIdentityRegistration"), 0.1f, [WeakActor](float)
	{
		AActor* CurrentActor = WeakActor.Get();
		if (!CurrentActor) return false;
		UWorld* ActorWorld = CurrentActor->GetWorld();
		if (!ActorWorld) return false;
		if (USceneModule* Scene = USceneModule::GetPtr(ActorWorld))
		{
			const FGuid CurrentId = ISceneActorInterface::Execute_GetActorID(CurrentActor);
			if (CurrentId.IsValid() && !Scene->QueueSceneActorRegistration(CurrentActor, CurrentId))
				UE_LOG(LogTemp, Error, TEXT("场景实体复制身份未能排队：%s id=%s"), *GetNameSafe(CurrentActor), *CurrentId.ToString());
			return false;
		}
		return true;
	});
}

void SceneActorIdentity::CancelReplication(FTSTicker::FDelegateHandle& RetryTicker)
{
	if (RetryTicker.IsValid()) FTSTicker::RemoveTicker(RetryTicker);
	RetryTicker.Reset();
}
