// Fill out your copyright notice in the Description page of Project Settings.


#include "ObjectPool/ObjectPool.h"

#include "ObjectPool/ObjectPoolInterface.h"

UObjectPool::UObjectPool()
{
	Limit = 100;
	Count = 0;
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
		SpawnImpl(Object);
		Count--;
	}
	else
	{
		Object = SpawnImpl(nullptr);
	}
	IObjectPoolInterface::Execute_OnSpawn(Object, InParams);
	return Object;
}

UObject* UObjectPool::SpawnImpl(UObject* InObject)
{
	if(!InObject)
	{
		InObject = NewObject<UObject>(GetTransientPackage(), Type);
	}
	else if(InObject->IsRooted())
	{
		InObject->RemoveFromRoot();
	}
	return InObject;
}

void UObjectPool::Despawn(UObject* InObject, bool bRecovery)
{
	IObjectPoolInterface::Execute_OnDespawn(InObject, bRecovery);
	if(bRecovery && Count < Limit)
	{
		DespawnImpl(InObject, true);
		Queue.Enqueue(InObject);
		Count++;
	}
	else
	{
		DespawnImpl(InObject, false);
	}
}

void UObjectPool::DespawnImpl(UObject* InObject, bool bRecovery)
{
	if(!bRecovery)
	{
		if(InObject->IsRooted())
		{
			InObject->RemoveFromRoot();
		}
	}
	else
	{
		if(!InObject->IsRooted())
		{
			InObject->AddToRoot();
		}
	}
}

void UObjectPool::Clear()
{
	UObject* Object;
	while(Queue.Dequeue(Object))
	{
		if(Object)
		{
			DespawnImpl(Object, false);
		}
	}
	Queue.Empty();
}
