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

void UMainModuleBPLibrary::ResetModuleByClass(TSubclassOf<AModuleBase> InModuleClass)
{
	if(AMainModule* MainModule = GetMainModule())
	{
		MainModule->ResetModuleByClass(InModuleClass);
	}
}

void UMainModuleBPLibrary::ResetModuleByName(const FName InModuleName)
{
	if(AMainModule* MainModule = GetMainModule())
	{
		MainModule->ResetModuleByName(InModuleName);
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
		MainModule->Pause();
	}
}

void UMainModuleBPLibrary::UnPauseMainModule()
{
	if(AMainModule* MainModule = GetMainModule())
	{
		MainModule->UnPause();
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

TArray<AModuleBase*> UMainModuleBPLibrary::GetAllModule(bool bInEditor)
{
	return AMainModule::GetAllModule(bInEditor);
}

AModuleBase* UMainModuleBPLibrary::GetModuleByClass(TSubclassOf<AModuleBase> InModuleClass, bool bInEditor)
{
	return AMainModule::GetModuleByClass(bInEditor, InModuleClass);
}

AModuleBase* UMainModuleBPLibrary::GetModuleByName(const FName InModuleName, bool bInEditor)
{
	return AMainModule::GetModuleByName<AModuleBase>(InModuleName, bInEditor);
}

UModuleNetworkComponentBase* UMainModuleBPLibrary::GetModuleNetworkComponentByClass(TSubclassOf<UModuleNetworkComponentBase> InModuleNetworkComponentClass, bool bInEditor)
{
	return AMainModule::GetModuleNetworkComponentByClass<UModuleNetworkComponentBase>(bInEditor, InModuleNetworkComponentClass);
}
