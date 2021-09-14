// Fill out your copyright notice in the Description page of Project Settings.


#include "Network/NetworkModuleBPLibrary.h"

#include "Kismet/GameplayStatics.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"
#include "Network/NetworkModule.h"
#include "Network/NetworkModuleNetworkComponent.h"

ANetworkModule* UNetworkModuleBPLibrary::GetNetworkModule(UObject* InWorldContext)
{
	if (!NetworkModuleInst || !NetworkModuleInst->IsValidLowLevel())
	{
		if(AMainModule* MainModule = UMainModuleBPLibrary::GetMainModule(InWorldContext))
		{
			NetworkModuleInst = MainModule->GetModuleByClass<ANetworkModule>();
		}
	}
	return NetworkModuleInst;
}

UNetworkModuleNetworkComponent* UNetworkModuleBPLibrary::GetNetworkModuleNetworkComponent(UObject* InWorldContext)
{
	if(!InWorldContext) return nullptr;
	
	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(InWorldContext, 0))
	{
		return Cast<UNetworkModuleNetworkComponent>(PlayerController->FindComponentByClass(UNetworkModuleNetworkComponent::StaticClass()));
	}
	return nullptr;
}

bool UNetworkModuleBPLibrary::ExecuteObjectFunc(UObject* InWorldContext, UObject* InObject, const FName& InFuncName, void* Params)
{
	if(ANetworkModule* NetworkModule = GetNetworkModule(InWorldContext))
	{
		return NetworkModule->ExecuteObjectFunc(InObject, InFuncName, Params);
	}
	return false;
}

void UNetworkModuleBPLibrary::ServerExecuteActorFunc(UObject* InWorldContext, AActor* InActor, FName InFuncName)
{
	if(ANetworkModule* NetworkModule = GetNetworkModule(InWorldContext))
	{
		NetworkModule->ServerExecuteActorFunc(InActor, InFuncName);
	}
}

void UNetworkModuleBPLibrary::ServerExecuteActorFuncOneParam(UObject* InWorldContext, AActor* InActor, FName InFuncName, FParameter InParam)
{
	if(ANetworkModule* NetworkModule = GetNetworkModule(InWorldContext))
	{
		NetworkModule->ServerExecuteActorFuncOneParam(InActor, InFuncName, InParam);
	}
}

void UNetworkModuleBPLibrary::ServerExecuteActorFuncTwoParam(UObject* InWorldContext, AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2)
{
	if(ANetworkModule* NetworkModule = GetNetworkModule(InWorldContext))
	{
		NetworkModule->ServerExecuteActorFuncTwoParam(InActor, InFuncName, InParam1, InParam2);
	}
}

void UNetworkModuleBPLibrary::ServerExecuteActorFuncThreeParam(UObject* InWorldContext, AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3)
{
	if(ANetworkModule* NetworkModule = GetNetworkModule(InWorldContext))
	{
		NetworkModule->ServerExecuteActorFuncThreeParam(InActor, InFuncName, InParam1, InParam2, InParam3);
	}
}

void UNetworkModuleBPLibrary::ServerExecuteActorFuncFourParam(UObject* InWorldContext, AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3, FParameter InParam4)
{
	if(ANetworkModule* NetworkModule = GetNetworkModule(InWorldContext))
	{
		NetworkModule->ServerExecuteActorFuncFourParam(InActor, InFuncName, InParam1, InParam2, InParam3, InParam4);
	}
}

void UNetworkModuleBPLibrary::ServerExecuteActorFuncFiveParam(UObject* InWorldContext, AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3, FParameter InParam4, FParameter InParam5)
{
	if(ANetworkModule* NetworkModule = GetNetworkModule(InWorldContext))
	{
		NetworkModule->ServerExecuteActorFuncFiveParam(InActor, InFuncName, InParam1, InParam2, InParam3, InParam4, InParam5);
	}
}
