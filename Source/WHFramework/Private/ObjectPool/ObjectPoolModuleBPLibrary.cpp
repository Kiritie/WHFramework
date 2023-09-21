// Fill out your copyright notice in the Description page of Project Settings.


#include "ObjectPool/ObjectPoolModuleBPLibrary.h"

UObject* UObjectPoolModuleBPLibrary::SpawnObject(TSubclassOf<UObject> InType, const TArray<FParameter>& InParams)
{
	if(AObjectPoolModule* ObjectPoolModule = AObjectPoolModule::Get())
	{
		return ObjectPoolModule->SpawnObject(InType, InParams);
	}
	return nullptr;
}

void UObjectPoolModuleBPLibrary::DespawnObject(UObject* InObject, bool bRecovery)
{
	if(AObjectPoolModule* ObjectPoolModule = AObjectPoolModule::Get())
	{
		ObjectPoolModule->DespawnObject(InObject, bRecovery);
	}
}

void UObjectPoolModuleBPLibrary::DespawnObjects(TArray<UObject*> InObjects, bool bRecovery)
{
	if(AObjectPoolModule* ObjectPoolModule = AObjectPoolModule::Get())
	{
		ObjectPoolModule->DespawnObjects(InObjects, bRecovery);
	}
}

void UObjectPoolModuleBPLibrary::ClearObject(TSubclassOf<UObject> InType)
{
	if(AObjectPoolModule* ObjectPoolModule = AObjectPoolModule::Get())
	{
		ObjectPoolModule->ClearObject(InType);
	}
}

void UObjectPoolModuleBPLibrary::ClearAllObject()
{
	if(AObjectPoolModule* ObjectPoolModule = AObjectPoolModule::Get())
	{
		ObjectPoolModule->ClearAllObject();
	}
}
