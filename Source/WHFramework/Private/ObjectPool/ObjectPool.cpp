// Fill out your copyright notice in the Description page of Project Settings.


#include "ObjectPool/ObjectPool.h"

#include "ObjectPool/ObjectPoolInterface.h"

UObjectPool::UObjectPool()
{
	Limit = 100;
	Type = nullptr;
	List = TArray<UObject*>();
}

void UObjectPool::Initialize(int32 InLimit, TSubclassOf<UObject> InType)
{
	Limit = InLimit;
	Type = InType;
}

UObject* UObjectPool::Spawn()
{
	UObject* Object;
	if(GetCount() > 0)
	{
		Object = List[0];
		List.RemoveAt(0);
	}
	else
	{
		Object = NewObject<UObject>(GetOuter(), Type);
	}
	if(IObjectPoolInterface* Interface = Cast<IObjectPoolInterface>(Object))
	{
		Interface->Execute_OnSpawn(Object);
	}
	return Object;
}

void UObjectPool::Despawn(UObject* InObject)
{
	if(GetCount() >= Limit)
	{
		InObject->ConditionalBeginDestroy();
	}
	else
	{
		List.Add(InObject);
	}
	if(IObjectPoolInterface* Interface = Cast<IObjectPoolInterface>(InObject))
	{
		Interface->Execute_OnDespawn(InObject);
	}
}

void UObjectPool::Clear()
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

int32 UObjectPool::GetLimit() const
{
	return Limit;
}

int32 UObjectPool::GetCount() const
{
	return List.Num();
}

TArray<UObject*> UObjectPool::GetQueue() const
{
	return List;
}
