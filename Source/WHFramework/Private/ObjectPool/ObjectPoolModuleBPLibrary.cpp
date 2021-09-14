// Fill out your copyright notice in the Description page of Project Settings.


#include "ObjectPool/ObjectPoolModuleBPLibrary.h"

#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"

AObjectPoolModule* UObjectPoolModuleBPLibrary::GetObjectPoolModule(UObject* InWorldContext)
{
	if (!ObjectPoolModuleInst || !ObjectPoolModuleInst->IsValidLowLevel())
	{
		if(AMainModule* MainModule = UMainModuleBPLibrary::GetMainModule(InWorldContext))
		{
			ObjectPoolModuleInst = MainModule->GetModuleByClass<AObjectPoolModule>();
		}
	}
	return ObjectPoolModuleInst;
}

UObject* UObjectPoolModuleBPLibrary::K2_SpawnObject(UObject* InWorldContext, TSubclassOf<UObject> InType)
{
	if(AObjectPoolModule* ObjectPoolModule = GetObjectPoolModule(InWorldContext))
	{
		return ObjectPoolModule->K2_SpawnObject(InType);
	}
	return nullptr;
}

void UObjectPoolModuleBPLibrary::DespawnObject(UObject* InWorldContext, UObject* InObject)
{
	if(AObjectPoolModule* ObjectPoolModule = GetObjectPoolModule(InWorldContext))
	{
		ObjectPoolModule->DespawnObject(InObject);
	}
}

void UObjectPoolModuleBPLibrary::ClearAllObject(UObject* InWorldContext)
{
	if(AObjectPoolModule* ObjectPoolModule = GetObjectPoolModule(InWorldContext))
	{
		ObjectPoolModule->ClearAllObject();
	}
}
