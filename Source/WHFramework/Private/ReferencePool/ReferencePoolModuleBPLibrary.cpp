// Fill out your copyright notice in the Description page of Project Settings.


#include "ReferencePool/ReferencePoolModuleBPLibrary.h"

#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"
#include "ReferencePool/ReferencePoolModule.h"

AActor* UReferencePoolModuleBPLibrary::K2_SpawnReference(TSubclassOf<AActor> InType)
{
	if(AReferencePoolModule* ReferencePoolModule = AMainModule::GetModuleByClass<AReferencePoolModule>())
	{
		return ReferencePoolModule->K2_SpawnReference(InType);
	}
	return nullptr;
}

void UReferencePoolModuleBPLibrary::DespawnReference(AActor* InActor)
{
	if(AReferencePoolModule* ReferencePoolModule = AMainModule::GetModuleByClass<AReferencePoolModule>())
	{
		ReferencePoolModule->DespawnReference(InActor);
	}
}

void UReferencePoolModuleBPLibrary::ClearAllActor()
{
	if(AReferencePoolModule* ReferencePoolModule = AMainModule::GetModuleByClass<AReferencePoolModule>())
	{
		ReferencePoolModule->ClearAllActor();
	}
}
