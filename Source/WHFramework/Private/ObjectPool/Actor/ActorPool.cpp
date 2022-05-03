// Fill out your copyright notice in the Description page of Project Settings.


#include "ObjectPool/Actor/ActorPool.h"

#include "ObjectPool/ObjectPoolInterface.h"

UActorPool::UActorPool()
{
}

UObject* UActorPool::SpawnImpl()
{
	FActorSpawnParameters ActorSpawnParameters;
	ActorSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	UObject* Object = GetWorld()->SpawnActor<AActor>(Type, ActorSpawnParameters);
	return Object;
}

void UActorPool::DespawnImpl(UObject* InObject)
{
	if(AActor* Actor = Cast<AActor>(InObject))
	{
		Actor->Destroy();
	}
}
