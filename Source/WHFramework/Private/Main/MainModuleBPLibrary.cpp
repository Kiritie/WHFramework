// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/MainModuleBPLibrary.h"

#include "Kismet/GameplayStatics.h"
#include "Main/MainModule.h"

void UMainModuleBPLibrary::PauseAllModule()
{
	if(AMainModule* MainModule = GetMainModule())
	{
		MainModule->PauseModules();
	}
}

void UMainModuleBPLibrary::UnPauseAllModule()
{
	if(AMainModule* MainModule = GetMainModule())
	{
		MainModule->UnPauseModules();
	}
}

AMainModule* UMainModuleBPLibrary::GetMainModule(bool bInEditor)
{
	return AMainModule::Get(bInEditor);
}

TArray<TScriptInterface<IModule>> UMainModuleBPLibrary::GetAllModule(bool bInEditor)
{
	return AMainModule::GetAllModule(bInEditor);
}

AModuleBase* UMainModuleBPLibrary::GetModuleByClass(TSubclassOf<AModuleBase> InModuleClass, bool bInEditor)
{
	return AMainModule::GetModuleByClass<AModuleBase>(bInEditor, InModuleClass);
}

TScriptInterface<IModule> UMainModuleBPLibrary::GetModuleByName(const FName InModuleName, bool bInEditor)
{
	return AMainModule::GetModuleByName<UObject>(InModuleName, bInEditor);
}

UModuleNetworkComponent* UMainModuleBPLibrary::GetModuleNetworkComponentByClass( TSubclassOf<UModuleNetworkComponent> InModuleClass, bool bInEditor)
{
	return AMainModule::GetModuleNetworkComponentByClass<UModuleNetworkComponent>(bInEditor, InModuleClass);
}
