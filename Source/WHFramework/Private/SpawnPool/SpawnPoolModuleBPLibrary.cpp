// Fill out your copyright notice in the Description page of Project Settings.


#include "SpawnPool/SpawnPoolModuleBPLibrary.h"

#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"
#include "SpawnPool/SpawnPoolModule.h"

ASpawnPoolModule* USpawnPoolModuleBPLibrary::SpawnPoolModuleInst = nullptr;

ASpawnPoolModule* USpawnPoolModuleBPLibrary::GetSpawnPoolModule(UObject* InWorldContext)
{
	if (!SpawnPoolModuleInst || !SpawnPoolModuleInst->IsValidLowLevel())
	{
		if(AMainModule* MainModule = UMainModuleBPLibrary::GetMainModule(InWorldContext))
		{
			SpawnPoolModuleInst = MainModule->GetModuleByClass<ASpawnPoolModule>();
		}
	}
	return SpawnPoolModuleInst;
}

AActor* USpawnPoolModuleBPLibrary::K2_SpawnActor(UObject* InWorldContext, TSubclassOf<AActor> InType)
{
	if(ASpawnPoolModule* SpawnPoolModule = GetSpawnPoolModule(InWorldContext))
	{
		return SpawnPoolModule->K2_SpawnActor(InType);
	}
	return nullptr;
}

void USpawnPoolModuleBPLibrary::DespawnActor(UObject* InWorldContext, AActor* InActor)
{
	if(ASpawnPoolModule* SpawnPoolModule = GetSpawnPoolModule(InWorldContext))
	{
		SpawnPoolModule->DespawnActor(InActor);
	}
}

void USpawnPoolModuleBPLibrary::ClearAllActor(UObject* InWorldContext)
{
	if(ASpawnPoolModule* SpawnPoolModule = GetSpawnPoolModule(InWorldContext))
	{
		SpawnPoolModule->ClearAllActor();
	}
}
