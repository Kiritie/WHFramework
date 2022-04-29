// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/MainModuleBPLibrary.h"

#include "Kismet/GameplayStatics.h"
#include "Main/MainModule.h"

AMainModule* UMainModuleBPLibrary::GetMainModule(bool bInEditor)
{
	return AMainModule::Get(bInEditor);
}

TArray<TScriptInterface<IModule>> UMainModuleBPLibrary::GetAllModules(bool bInEditor)
{
	return AMainModule::GetAllModules(bInEditor);
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
