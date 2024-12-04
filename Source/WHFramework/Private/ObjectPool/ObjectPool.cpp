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

UObject* UObjectPool::Spawn(UObject* InOwner, const TArray<FParameter>& InParams)
{
	UObject* Object;
	if(Count > 0)
	{
		Queue.Dequeue(Object);
		OnSpawn(Object);
		Count--;
	}
	else
	{
		Object = OnSpawn(nullptr);
	}
	if(Object)
	{
		IObjectPoolInterface::Execute_OnSpawn(Object, InOwner, InParams);
	}
	return Object;
}

void UObjectPool::Despawn(UObject* InObject, bool bRecovery)
{
	IObjectPoolInterface::Execute_OnDespawn(InObject, bRecovery);
	if(bRecovery && (Limit == -1 || Count < Limit))
	{
		OnDespawn(InObject, true);
		Queue.Enqueue(InObject);
		Count++;
	}
	else
	{
		OnDespawn(InObject, false);
	}
}

void UObjectPool::Clear()
{
	UObject* Object;
	while(Queue.Dequeue(Object))
	{
		if(Object)
		{
			OnDespawn(Object, false);
		}
	}
	Queue.Empty();
}

UObject* UObjectPool::OnSpawn(UObject* InObject)
{
	if(!InObject)
	{
		InObject = NewObject<UObject>(this, Type);
	}
	else if(InObject->IsRooted())
	{
		InObject->RemoveFromRoot();
	}
	return InObject;
}

void UObjectPool::OnDespawn(UObject* InObject, bool bRecovery)
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
