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
	return Actor;
}

void USpawnPool::Despawn(AActor* InActor)
{
	if(GetCount() >= Limit)
	{
		InActor->ConditionalBeginDestroy();
	}
	else
	{
		List.Add(InActor);
		if(ISpawnPoolInterface* Reference = Cast<ISpawnPoolInterface>(InActor))
		{
			Reference->Execute_Reset(InActor);
		}
	}
}

void USpawnPool::Clear()
{
	for (auto Iter : List)
	{
		if(Iter)
		{
			Iter->ConditionalBeginDestroy();
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
