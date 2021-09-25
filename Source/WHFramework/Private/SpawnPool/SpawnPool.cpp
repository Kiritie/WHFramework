// Fill out your copyright notice in the Description page of Project Settings.


#include "SpawnPool/SpawnPool.h"

#include "SpawnPool/SpawnPoolInterface.h"

USpawnPool::USpawnPool()
{
	Limit = 100;
	Type = nullptr;
	List = TArray<AActor*>();
}

void USpawnPool::Initialize(int32 InLimit, TSubclassOf<AActor> InType)
{
	Limit = InLimit;
	Type = InType;
}

AActor* USpawnPool::Spawn()
{
	AActor* Actor;
	if(GetCount() > 0)
	{
		Actor = List[0];
		List.RemoveAt(0);
	}
	else
	{
		FActorSpawnParameters ActorSpawnParameters;
		ActorSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Actor = GetWorld()->SpawnActor<AActor>(Type, ActorSpawnParameters);
	}
	if(ISpawnPoolInterface* Interface = Cast<ISpawnPoolInterface>(Actor))
	{
		Interface->Execute_OnSpawn(Actor);
	}
	return Actor;
}

void USpawnPool::Despawn(AActor* InActor)
{
	if(GetCount() >= Limit)
	{
		InActor->Destroy();
	}
	else
	{
		List.Add(InActor);
	}
	if(ISpawnPoolInterface* Interface = Cast<ISpawnPoolInterface>(InActor))
	{
		Interface->Execute_OnDespawn(InActor);
	}
}

void USpawnPool::Clear()
{
	for (auto Iter : List)
	{
		if(Iter)
		{
			Iter->Destroy();
		}
	}
	List.Empty();
}

int32 USpawnPool::GetLimit() const
{
	return Limit;
}

int32 USpawnPool::GetCount() const
{
	return List.Num();
}

TArray<AActor*> USpawnPool::GetQueue() const
{
	return List;
}
