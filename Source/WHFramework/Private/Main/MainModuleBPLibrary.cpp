// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/MainModuleBPLibrary.h"

#include "Kismet/GameplayStatics.h"
#include "Main/MainModule.h"

AMainModule* UMainModuleBPLibrary::GetMainModule()
{
	return AMainModule::Get();
}

TArray<TScriptInterface<IModule>> UMainModuleBPLibrary::GetAllModules()
{
	return AMainModule::GetAllModules();
}

AModuleBase* UMainModuleBPLibrary::GetModuleByClass(TSubclassOf<AModuleBase> InModuleClass)
{
	return AMainModule::GetModuleByClass<AModuleBase>(InModuleClass);
}

TScriptInterface<IModule> UMainModuleBPLibrary::GetModuleByName(const FName InModuleName)
{
	return AMainModule::GetModuleByName<UObject>(InModuleName);
}

UModuleNetworkComponent* UMainModuleBPLibrary::GetModuleNetworkComponentByClass( TSubclassOf<UModuleNetworkComponent> InModuleClass)
{
	return AMainModule::GetModuleNetworkComponentByClass<UModuleNetworkComponent>(InModuleClass);
}
