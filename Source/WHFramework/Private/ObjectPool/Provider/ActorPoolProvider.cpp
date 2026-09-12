#include "ObjectPool/Provider/ActorPoolProvider.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "ObjectPool/ObjectPoolBucket.h"
#include "ObjectPool/ObjectPoolModule.h"
#include "ObjectPool/ObjectPoolModuleTypes.h"

FActorPoolProvider::FActorPoolProvider(UObjectPoolModule& InModule)
	: Module(InModule)
{
}

UObject* FActorPoolProvider::Spawn(UClass* InClass, const FParameter& InParam)
{
	const FActorSpawnParameter* Parameter = InParam.GetPtr<FActorSpawnParameter>();
	UObject* WorldContext = Parameter ? Parameter->WorldContext.Get() : nullptr;
	if(!WorldContext && Parameter)
	{
		WorldContext = Parameter->Owner.Get();
	}

	UWorld* World = GEngine && WorldContext
		? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull)
		: Module.GetWorld();
	if(!World || World->bIsTearingDown)
	{
		return nullptr;
	}

	const FObjectPoolPolicy Policy = Module.GetPoolPolicy(InClass);
	AActor* Actor = Policy.bEnablePooling
		? Cast<AActor>(Module.FindOrAddGenericBucket(InClass, World, Policy.MaxIdle)->Acquire())
		: nullptr;

	if(!Actor)
	{
		FActorSpawnParameters SpawnParameters;
		if(Parameter)
		{
			SpawnParameters.Owner = Parameter->Owner;
			SpawnParameters.Instigator = Parameter->Instigator;
			SpawnParameters.SpawnCollisionHandlingOverride = Parameter->CollisionHandling;
		}

		Actor = World->SpawnActor<AActor>(
			InClass,
			Parameter ? Parameter->Transform : FTransform::Identity,
			SpawnParameters);
	}
	else
	{
		if(const FActorPoolState* State = ActorStates.Find(Actor))
		{
			Actor->SetActorHiddenInGame(State->bHidden);
			Actor->SetActorEnableCollision(State->bCollisionEnabled);
			Actor->SetActorTickEnabled(State->bTickEnabled);
		}

		Actor->SetActorTransform(Parameter ? Parameter->Transform : FTransform::Identity);
		Actor->SetOwner(Parameter ? Parameter->Owner.Get() : nullptr);
		Actor->SetInstigator(Parameter ? Parameter->Instigator.Get() : nullptr);
	}

	return Actor;
}

void FActorPoolProvider::PrepareDespawn(UObject* InObject)
{
	AActor* Actor = Cast<AActor>(InObject);
	if(!Actor || !Module.GetPoolPolicy(Actor->GetClass()).bEnablePooling)
	{
		return;
	}

	FActorPoolState& State = ActorStates.FindOrAdd(Actor);
	State.bHidden = Actor->IsHidden();
	State.bCollisionEnabled = Actor->GetActorEnableCollision();
	State.bTickEnabled = Actor->IsActorTickEnabled();
}

bool FActorPoolProvider::Despawn(UObject* InObject)
{
	AActor* Actor = Cast<AActor>(InObject);
	if(!Actor)
	{
		return false;
	}

	const FObjectPoolPolicy Policy = Module.GetPoolPolicy(Actor->GetClass());
	if(!Policy.bEnablePooling)
	{
		Actor->Destroy();
		return false;
	}

	Actor->SetActorHiddenInGame(true);
	Actor->SetActorEnableCollision(false);
	Actor->SetActorTickEnabled(false);

	if(Module.FindOrAddGenericBucket(Actor->GetClass(), Actor->GetWorld(), Policy.MaxIdle)->Release(Actor))
	{
		return true;
	}

	ActorStates.Remove(Actor);
	Actor->Destroy();
	return false;
}

void FActorPoolProvider::Destroy(UObject* InObject)
{
	if(AActor* Actor = Cast<AActor>(InObject))
	{
		Module.RemoveFromGenericBucket(Actor);
		ActorStates.Remove(Actor);
		if(IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
}

void FActorPoolProvider::ClearAll()
{
	ActorStates.Reset();
}
