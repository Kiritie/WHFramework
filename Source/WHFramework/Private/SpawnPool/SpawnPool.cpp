// Fill out your copyright notice in the Description page of Project Settings.


#include "SpawnPool/SpawnPool.h"

#include "SpawnPool/SpawnPoolInterface.h"

USpawnPool::USpawnPool()
{
	Limit = 100;
	Type = nullptr;
}

void USpawnPool::Initialize(int32 InLimit, TSubclassOf<AActor> InType)
{
	Limit = InLimit;
	Type = InType;
}

AActor* USpawnPool::Spawn()
{
	AActor* Actor;
	if(Count > 0)
	{
		Queue.Dequeue(Actor);
		Count--;
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
	if(Count >= Limit)
	{
		InActor->Destroy();
	}
	else
	{
		Queue.Enqueue(InActor);
		Count++;
	}
	if(ISpawnPoolInterface* Interface = Cast<ISpawnPoolInterface>(InActor))
	{
		Interface->Execute_OnDespawn(InActor);
	}
}

void USpawnPool::Clear()
{
	AActor* Actor;
	while(Queue.Dequeue(Actor))
	{
		if(Actor)
		{
			Actor->Destroy();
		}
	}
	Queue.Empty();
}

int32 USpawnPool::GetLimit() const
{
	return Limit;
}

int32 USpawnPool::GetCount() const
{
	return Count;
}
