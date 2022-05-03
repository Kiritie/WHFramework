// Fill out your copyright notice in the Description page of Project Settings.


#include "ReferencePool/ReferencePool.h"

#include "ReferencePool/ReferencePoolInterface.h"

UReferencePool::UReferencePool()
{
	Limit = 100;
	Type = nullptr;
}

void UReferencePool::Initialize(int32 InLimit, TSubclassOf<AActor> InType)
{
	Limit = InLimit;
	Type = InType;
}

AActor* UReferencePool::Spawn()
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
	IReferencePoolInterface::Execute_OnSpawn(Actor);
	return Actor;
}

void UReferencePool::Despawn(AActor* InActor)
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
	IReferencePoolInterface::Execute_OnDespawn(InActor);
}

void UReferencePool::Clear()
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

int32 UReferencePool::GetLimit() const
{
	return Limit;
}

int32 UReferencePool::GetCount() const
{
	return Count;
}
