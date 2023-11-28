// Fill out your copyright notice in the Description page of Project Settings.


#include "ObjectPool/ObjectPoolModuleStatics.h"

UObject* UObjectPoolModuleStatics::SpawnObject(TSubclassOf<UObject> InType, UObject* InOwner, const TArray<FParameter>& InParams)
{
	return UObjectPoolModule::Get().SpawnObject(InType, InOwner, InParams);
}

void UObjectPoolModuleStatics::DespawnObject(UObject* InObject, bool bRecovery)
{
	UObjectPoolModule::Get().DespawnObject(InObject, bRecovery);
}

void UObjectPoolModuleStatics::DespawnObjects(TArray<UObject*> InObjects, bool bRecovery)
{
	UObjectPoolModule::Get().DespawnObjects(InObjects, bRecovery);
}

void UObjectPoolModuleStatics::ClearObject(TSubclassOf<UObject> InType)
{
	UObjectPoolModule::Get().ClearObject(InType);
}

void UObjectPoolModuleStatics::ClearAllObject()
{
	UObjectPoolModule::Get().ClearAllObject();
}
