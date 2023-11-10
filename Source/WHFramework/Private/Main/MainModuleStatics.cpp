// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/MainModuleStatics.h"

#include "Main/MainModule.h"

void UMainModuleStatics::RunModuleByClass(TSubclassOf<UModuleBase> InClass)
{
	if(AMainModule* MainModule = GetMainModule())
	{
		MainModule->RunModuleByClass(InClass);
	}
}

void UMainModuleStatics::RunModuleByName(const FName InName)
{
	if(AMainModule* MainModule = GetMainModule())
	{
		MainModule->RunModuleByName(InName);
	}
}

void UMainModuleStatics::ResetModuleByClass(TSubclassOf<UModuleBase> InClass)
{
	if(AMainModule* MainModule = GetMainModule())
	{
		MainModule->ResetModuleByClass(InClass);
	}
}

void UMainModuleStatics::ResetModuleByName(const FName InName)
{
	if(AMainModule* MainModule = GetMainModule())
	{
		MainModule->ResetModuleByName(InName);
	}
}

void UMainModuleStatics::PauseModuleByClass(TSubclassOf<UModuleBase> InClass)
{
	if(AMainModule* MainModule = GetMainModule())
	{
		MainModule->PauseModuleByClass(InClass);
	}
}

void UMainModuleStatics::PauseModuleByName(const FName InName)
{
	if(AMainModule* MainModule = GetMainModule())
	{
		MainModule->PauseModuleByName(InName);
	}
}

void UMainModuleStatics::UnPauseModuleByClass(TSubclassOf<UModuleBase> InClass)
{
	if(AMainModule* MainModule = GetMainModule())
	{
		MainModule->UnPauseModuleByClass(InClass);
	}
}

void UMainModuleStatics::UnPauseModuleByName(const FName InName)
{
	if(AMainModule* MainModule = GetMainModule())
	{
		MainModule->UnPauseModuleByName(InName);
	}
}

void UMainModuleStatics::PauseAllModule()
{
	if(AMainModule* MainModule = GetMainModule())
	{
		MainModule->PauseModules();
	}
}

void UMainModuleStatics::UnPauseAllModule()
{
	if(AMainModule* MainModule = GetMainModule())
	{
		MainModule->UnPauseModules();
	}
}

AMainModule* UMainModuleStatics::GetMainModule(bool bInEditor)
{
	return AMainModule::Get(bInEditor);
}

TArray<UModuleBase*> UMainModuleStatics::GetAllModule(bool bInEditor)
{
	return AMainModule::GetAllModule(bInEditor);
}

UModuleBase* UMainModuleStatics::GetModuleByClass(TSubclassOf<UModuleBase> InClass, bool bInEditor)
{
	return &AMainModule::GetModuleByClass(bInEditor, InClass);
}

UModuleBase* UMainModuleStatics::GetModuleByName(const FName InName, bool bInEditor)
{
	return &AMainModule::GetModuleByName<UModuleBase>(InName, bInEditor);
}

UModuleNetworkComponentBase* UMainModuleStatics::GetModuleNetworkComponentByClass(TSubclassOf<UModuleNetworkComponentBase> InClass)
{
	return AMainModule::GetModuleNetworkComponentByClass<UModuleNetworkComponentBase>(InClass);
}
