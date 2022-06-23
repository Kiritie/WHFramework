// Fill out your copyright notice in the Description page of Project Settings.


#include "ObjectPool/ObjectPoolModuleBPLibrary.h"

#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"

UObject* UObjectPoolModuleBPLibrary::SpawnObject(TSubclassOf<UObject> InType, const TArray<FParameter>& InParams)
{
	if(AObjectPoolModule* ObjectPoolModule = AMainModule::GetModuleByClass<AObjectPoolModule>())
	{
		return ObjectPoolModule->SpawnObject(InType, InParams);
	}
	return nullptr;
}

void UObjectPoolModuleBPLibrary::DespawnObject(UObject* InObject)
{
	if(AObjectPoolModule* ObjectPoolModule = AMainModule::GetModuleByClass<AObjectPoolModule>())
	{
		ObjectPoolModule->DespawnObject(InObject);
	}
}

void UObjectPoolModuleBPLibrary::ClearObject(TSubclassOf<UObject> InType)
{
	if(AObjectPoolModule* ObjectPoolModule = AMainModule::GetModuleByClass<AObjectPoolModule>())
	{
		ObjectPoolModule->ClearObject(InType);
	}
}

void UObjectPoolModuleBPLibrary::ClearAllObject()
{
	if(AObjectPoolModule* ObjectPoolModule = AMainModule::GetModuleByClass<AObjectPoolModule>())
	{
		ObjectPoolModule->ClearAllObject();
	}
}
