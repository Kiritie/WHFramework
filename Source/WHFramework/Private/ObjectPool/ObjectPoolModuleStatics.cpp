// Fill out your copyright notice in the Description page of Project Settings.


#include "ObjectPool/ObjectPoolModuleStatics.h"

UObject* UObjectPoolModuleStatics::SpawnObject(TSubclassOf<UObject> InType, const TArray<FParameter>& InParams)
{
	if(UObjectPoolModule* ObjectPoolModule = UObjectPoolModule::Get())
	{
		return ObjectPoolModule->SpawnObject(InType, InParams);
	}
	return nullptr;
}

void UObjectPoolModuleStatics::DespawnObject(UObject* InObject, bool bRecovery)
{
	if(UObjectPoolModule* ObjectPoolModule = UObjectPoolModule::Get())
	{
		ObjectPoolModule->DespawnObject(InObject, bRecovery);
	}
}

void UObjectPoolModuleStatics::DespawnObjects(TArray<UObject*> InObjects, bool bRecovery)
{
	if(UObjectPoolModule* ObjectPoolModule = UObjectPoolModule::Get())
	{
		ObjectPoolModule->DespawnObjects(InObjects, bRecovery);
	}
}

void UObjectPoolModuleStatics::ClearObject(TSubclassOf<UObject> InType)
{
	if(UObjectPoolModule* ObjectPoolModule = UObjectPoolModule::Get())
	{
		ObjectPoolModule->ClearObject(InType);
	}
}

void UObjectPoolModuleStatics::ClearAllObject()
{
	if(UObjectPoolModule* ObjectPoolModule = UObjectPoolModule::Get())
	{
		ObjectPoolModule->ClearAllObject();
	}
}
