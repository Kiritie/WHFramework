// Fill out your copyright notice in the Description page of Project Settings.


#include "ReferencePool/ReferencePool.h"

#include "ReferencePool/ReferencePoolInterface.h"

UReferencePool::UReferencePool()
{
	Type = nullptr;
	Object = nullptr;
}

void UReferencePool::Initialize(TSubclassOf<UObject> InType)
{
	Type = InType;
}

UObject& UReferencePool::Get()
{
	if(!Object)
	{
		Object = NewObject<UObject>(this, Type);
	}
	return *Object;
}

void UReferencePool::Set(UObject* InObject)
{
	Object = InObject;
}

void UReferencePool::Reset()
{
	if(Object)
	{
		IReferencePoolInterface::Execute_OnReset(Object);
	}
}

void UReferencePool::Clear()
{
	if(Object)
	{
		Object->ConditionalBeginDestroy();
		Object = nullptr;
	}
}

bool UReferencePool::IsEmpty() const
{
	return !Object;
}
