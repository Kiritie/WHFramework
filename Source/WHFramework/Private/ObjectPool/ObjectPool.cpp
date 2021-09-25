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

UObject* UObjectPool::Spawn()
{
	UObject* Object;
	if(Count > 0)
	{
		Queue.Dequeue(Object);
		Count--;
	}
	else
	{
		Object = NewObject<UObject>(GetTransientPackage(), Type);
		Object->AddToRoot();
	}
	if(IObjectPoolInterface* Interface = Cast<IObjectPoolInterface>(Object))
	{
		Interface->Execute_OnSpawn(Object);
	}
	return Object;
}

void UObjectPool::Despawn(UObject* InObject)
{
	if(Count >= Limit)
	{
		InObject->RemoveFromRoot();
		InObject->ConditionalBeginDestroy();
	}
	else
	{
		Queue.Enqueue(InObject);
		Count++;
	}
	if(IObjectPoolInterface* Interface = Cast<IObjectPoolInterface>(InObject))
	{
		Interface->Execute_OnDespawn(InObject);
	}
}

void UObjectPool::Clear()
{
	UObject* Object;
	while(Queue.Dequeue(Object))
	{
		if(Object)
		{
			Object->RemoveFromRoot();
			Object->ConditionalBeginDestroy();
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
