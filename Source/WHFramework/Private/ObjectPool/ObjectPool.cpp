// Fill out your copyright notice in the Description page of Project Settings.


#include "ObjectPool/ObjectPool.h"

#include "ObjectPool/ObjectPoolInterface.h"

UObjectPool::UObjectPool()
{
	Limit = 100;
	Type = nullptr;
}

void UObjectPool::Initialize(int32 InLimit, TSubclassOf<UObject> InType)
{
	Limit = InLimit;
	Type = InType;
}

UObject* UObjectPool::Spawn(const TArray<FParameter>& InParams)
{
	UObject* Object;
	if(Count > 0)
	{
		Queue.Dequeue(Object);
		Count--;
	}
	else
	{
		Object = SpawnImpl();
	}
	IObjectPoolInterface::Execute_OnSpawn(Object, InParams);
	return Object;
}

UObject* UObjectPool::SpawnImpl()
{
	UObject* Object = NewObject<UObject>(GetTransientPackage(), Type);
	Object->AddToRoot();
	return Object;
}

void UObjectPool::Despawn(UObject* InObject)
{
	if(Count >= Limit)
	{
		DespawnImpl(InObject);
	}
	else
	{
		Queue.Enqueue(InObject);
		Count++;
	}
	IObjectPoolInterface::Execute_OnDespawn(InObject);
}

void UObjectPool::DespawnImpl(UObject* InObject)
{
	if(InObject->IsRooted())
	{
		InObject->RemoveFromRoot();
	}
	InObject->ConditionalBeginDestroy();
}

void UObjectPool::Clear()
{
	UObject* Object;
	while(Queue.Dequeue(Object))
	{
		if(Object)
		{
			DespawnImpl(Object);
		}
	}
	Queue.Empty();
}

int32 UObjectPool::GetLimit() const
{
	return Limit;
}

int32 UObjectPool::GetCount() const
{
	return Count;
}
