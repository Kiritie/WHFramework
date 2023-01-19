// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/MainModuleBPLibrary.h"

#include "Kismet/GameplayStatics.h"
#include "Main/MainModule.h"

void UMainModuleBPLibrary::RunModuleByClass(TSubclassOf<AModuleBase> InModuleClass)
{
	if(AMainModule* MainModule = GetMainModule())
	{
		MainModule->RunModuleByClass(InModuleClass);
	}
}

void UMainModuleBPLibrary::RunModuleByName(const FName InModuleName)
{
	if(AMainModule* MainModule = GetMainModule())
	{
		MainModule->RunModuleByName(InModuleName);
	}
}

void UMainModuleBPLibrary::PauseModuleByClass(TSubclassOf<AModuleBase> InModuleClass)
{
	if(AMainModule* MainModule = GetMainModule())
	{
		MainModule->PauseModuleByClass(InModuleClass);
	}
}

void UMainModuleBPLibrary::PauseModuleByName(const FName InModuleName)
{
	if(AMainModule* MainModule = GetMainModule())
	{
		MainModule->PauseModuleByName(InModuleName);
	}
}

void UMainModuleBPLibrary::UnPauseModuleByClass(TSubclassOf<AModuleBase> InModuleClass)
{
	if(AMainModule* MainModule = GetMainModule())
	{
		MainModule->UnPauseModuleByClass(InModuleClass);
	}
}

void UMainModuleBPLibrary::UnPauseModuleByName(const FName InModuleName)
{
	if(AMainModule* MainModule = GetMainModule())
	{
		MainModule->UnPauseModuleByName(InModuleName);
	}
}

void UMainModuleBPLibrary::PauseMainModule()
{
	if(AMainModule* MainModule = GetMainModule())
	{
		MainModule->Execute_Pause(MainModule);
	}
}

void UMainModuleBPLibrary::UnPauseMainModule()
{
	if(AMainModule* MainModule = GetMainModule())
	{
		MainModule->Execute_UnPause(MainModule);
	}
}

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
	return AMainModule::GetModuleByClass(bInEditor, InModuleClass);
}

TScriptInterface<IModule> UMainModuleBPLibrary::GetModuleByName(const FName InModuleName, bool bInEditor)
{
	return AMainModule::GetModuleByName<UObject>(InModuleName, bInEditor);
}

UModuleNetworkComponent* UMainModuleBPLibrary::GetModuleNetworkComponentByClass(TSubclassOf<UModuleNetworkComponent> InModuleNetworkComponentClass, bool bInEditor)
{
	return AMainModule::GetModuleNetworkComponentByClass<UModuleNetworkComponent>(bInEditor, InModuleNetworkComponentClass);
}
