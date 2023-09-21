// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/MainModuleBPLibrary.h"

#include "Main/MainModule.h"

void UMainModuleBPLibrary::RunModuleByClass(TSubclassOf<AModuleBase> InClass)
{
	if(AMainModule* MainModule = GetMainModule())
	{
		MainModule->RunModuleByClass(InClass);
	}
}

void UMainModuleBPLibrary::RunModuleByName(const FName InName)
{
	if(AMainModule* MainModule = GetMainModule())
	{
		MainModule->RunModuleByName(InName);
	}
}

void UMainModuleBPLibrary::ResetModuleByClass(TSubclassOf<AModuleBase> InClass)
{
	if(AMainModule* MainModule = GetMainModule())
	{
		MainModule->ResetModuleByClass(InClass);
	}
}

void UMainModuleBPLibrary::ResetModuleByName(const FName InName)
{
	if(AMainModule* MainModule = GetMainModule())
	{
		MainModule->ResetModuleByName(InName);
	}
}

void UMainModuleBPLibrary::PauseModuleByClass(TSubclassOf<AModuleBase> InClass)
{
	if(AMainModule* MainModule = GetMainModule())
	{
		MainModule->PauseModuleByClass(InClass);
	}
}

void UMainModuleBPLibrary::PauseModuleByName(const FName InName)
{
	if(AMainModule* MainModule = GetMainModule())
	{
		MainModule->PauseModuleByName(InName);
	}
}

void UMainModuleBPLibrary::UnPauseModuleByClass(TSubclassOf<AModuleBase> InClass)
{
	if(AMainModule* MainModule = GetMainModule())
	{
		MainModule->UnPauseModuleByClass(InClass);
	}
}

void UMainModuleBPLibrary::UnPauseModuleByName(const FName InName)
{
	if(AMainModule* MainModule = GetMainModule())
	{
		MainModule->UnPauseModuleByName(InName);
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

AModuleBase* UMainModuleBPLibrary::GetModuleByClass(TSubclassOf<AModuleBase> InClass, bool bInEditor)
{
	return AMainModule::GetModuleByClass(bInEditor, InClass);
}

AModuleBase* UMainModuleBPLibrary::GetModuleByName(const FName InName, bool bInEditor)
{
	return AMainModule::GetModuleByName<AModuleBase>(InName, bInEditor);
}

UModuleNetworkComponentBase* UMainModuleBPLibrary::GetModuleNetworkComponentByClass(TSubclassOf<UModuleNetworkComponentBase> InModuleNetworkComponentClass, bool bInEditor)
{
	return AMainModule::GetModuleNetworkComponentByClass<UModuleNetworkComponentBase>(bInEditor, InModuleNetworkComponentClass);
}
