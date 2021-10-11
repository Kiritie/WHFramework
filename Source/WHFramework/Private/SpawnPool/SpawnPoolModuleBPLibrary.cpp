// Fill out your copyright notice in the Description page of Project Settings.


#include "SpawnPool/SpawnPoolModuleBPLibrary.h"

#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"
#include "SpawnPool/SpawnPoolModule.h"

AActor* USpawnPoolModuleBPLibrary::K2_SpawnActor(TSubclassOf<AActor> InType)
{
	if(ASpawnPoolModule* SpawnPoolModule = AMainModule::GetModuleByClass<ASpawnPoolModule>())
	{
		return SpawnPoolModule->K2_SpawnActor(InType);
	}
	return nullptr;
}

void USpawnPoolModuleBPLibrary::DespawnActor(AActor* InActor)
{
	if(ASpawnPoolModule* SpawnPoolModule = AMainModule::GetModuleByClass<ASpawnPoolModule>())
	{
		SpawnPoolModule->DespawnActor(InActor);
	}
}

void USpawnPoolModuleBPLibrary::ClearAllActor()
{
	if(ASpawnPoolModule* SpawnPoolModule = AMainModule::GetModuleByClass<ASpawnPoolModule>())
	{
		SpawnPoolModule->ClearAllActor();
	}
}
