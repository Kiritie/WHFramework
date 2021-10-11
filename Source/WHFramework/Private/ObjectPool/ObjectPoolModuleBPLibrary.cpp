// Fill out your copyright notice in the Description page of Project Settings.


#include "ObjectPool/ObjectPoolModuleBPLibrary.h"

#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"

UObject* UObjectPoolModuleBPLibrary::K2_SpawnObject(TSubclassOf<UObject> InType)
{
	if(AObjectPoolModule* ObjectPoolModule = AMainModule::GetModuleByClass<AObjectPoolModule>())
	{
		return ObjectPoolModule->K2_SpawnObject(InType);
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

void UObjectPoolModuleBPLibrary::ClearAllObject()
{
	if(AObjectPoolModule* ObjectPoolModule = AMainModule::GetModuleByClass<AObjectPoolModule>())
	{
		ObjectPoolModule->ClearAllObject();
	}
}
