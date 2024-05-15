// Fill out your copyright notice in the Description page of Project Settings.


#include "ObjectPool/ObjectPoolModuleStatics.h"

bool UObjectPoolModuleStatics::HasObject(TSubclassOf<UObject> InType, bool bInEditor)
{
	return UObjectPoolModule::Get(bInEditor).HasObject(InType);
}

UObject* UObjectPoolModuleStatics::SpawnObject(TSubclassOf<UObject> InType, UObject* InOwner, const TArray<FParameter>& InParams, bool bInEditor)
{
	return UObjectPoolModule::Get(bInEditor).SpawnObject(InType, InOwner, InParams);
}

void UObjectPoolModuleStatics::DespawnObject(UObject* InObject, bool bRecovery, bool bInEditor)
{
	UObjectPoolModule::Get(bInEditor).DespawnObject(InObject, bRecovery);
}

void UObjectPoolModuleStatics::DespawnObjects(TArray<UObject*> InObjects, bool bRecovery, bool bInEditor)
{
	UObjectPoolModule::Get(bInEditor).DespawnObjects(InObjects, bRecovery);
}

void UObjectPoolModuleStatics::ClearObject(TSubclassOf<UObject> InType, bool bInEditor)
{
	UObjectPoolModule::Get(bInEditor).ClearObject(InType);
}

void UObjectPoolModuleStatics::ClearAllObject(bool bInEditor)
{
	UObjectPoolModule::Get(bInEditor).ClearAllObject();
}
