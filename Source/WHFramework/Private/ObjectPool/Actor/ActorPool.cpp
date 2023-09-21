// Fill out your copyright notice in the Description page of Project Settings.


#include "ObjectPool/Actor/ActorPool.h"

#include "Main/MainModule.h"

UActorPool::UActorPool()
{
}

UObject* UActorPool::OnSpawn(UObject* InObject)
{
	if(!InObject)
	{
		FActorSpawnParameters ActorSpawnParameters;
		ActorSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		InObject = GetWorld()->SpawnActor<AActor>(Type, ActorSpawnParameters);
	}
	return InObject;
}

void UActorPool::OnDespawn(UObject* InObject, bool bRecovery)
{
	if(!bRecovery && AMainModule::Get()->GetModuleState() != EModuleState::Terminated)
	{
		if(AActor* Actor = Cast<AActor>(InObject))
		{
			Actor->Destroy();
		}
	}
}
