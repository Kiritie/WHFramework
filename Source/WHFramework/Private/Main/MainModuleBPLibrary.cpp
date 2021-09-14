// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/MainModuleBPLibrary.h"

#include "Kismet/GameplayStatics.h"
#include "Main/MainModule.h"

AMainModule* UMainModuleBPLibrary::GetMainModule(const UObject* InWorldContext)
{
	if (!MainModuleInst || !MainModuleInst->IsValidLowLevel())
	{
		if(InWorldContext && InWorldContext->IsValidLowLevel())
		{
			MainModuleInst = Cast<AMainModule>(UGameplayStatics::GetActorOfClass(InWorldContext, AMainModule::StaticClass()));
		}
	}
	return MainModuleInst;
}

AModuleBase* UMainModuleBPLibrary::K2_GetModuleByClass(const UObject* InWorldContext, TSubclassOf<AModuleBase> InModuleClass)
{
	if(AMainModule* MainModule = GetMainModule(InWorldContext))
	{
		return MainModule->K2_GetModuleByClass(InModuleClass);
	}
	return nullptr;
}

TScriptInterface<IModule> UMainModuleBPLibrary::K2_GetModuleByName(const UObject* InWorldContext, const FName InModuleName)
{
	if(AMainModule* MainModule = GetMainModule(InWorldContext))
	{
		return MainModule->K2_GetModuleByName(InModuleName);
	}
	return nullptr;
}
