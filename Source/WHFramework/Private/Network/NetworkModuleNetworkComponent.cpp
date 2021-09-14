// Fill out your copyright notice in the Description page of Project Settings.

#include "Network/NetworkModuleNetworkComponent.h"

#include "Network/NetworkModule.h"
#include "Network/NetworkModuleBPLibrary.h"

UNetworkModuleNetworkComponent::UNetworkModuleNetworkComponent()
{
	
}

bool UNetworkModuleNetworkComponent::ServerExecuteActorFunc_Validate(AActor* InActor, FName InFuncName) { return true; }
void UNetworkModuleNetworkComponent::ServerExecuteActorFunc_Implementation(AActor* InActor, FName InFuncName)
{
	if(ANetworkModule* NetworkModule = UNetworkModuleBPLibrary::GetNetworkModule(this))
    {
    	NetworkModule->ServerExecuteActorFunc(InActor, InFuncName);
    }
}

bool UNetworkModuleNetworkComponent::ServerExecuteActorFuncOneParam_Validate(AActor* InActor, FName InFuncName, FParameter InParam) { return true; }
void UNetworkModuleNetworkComponent::ServerExecuteActorFuncOneParam_Implementation(AActor* InActor, FName InFuncName, FParameter InParam)
{
	if(ANetworkModule* NetworkModule = UNetworkModuleBPLibrary::GetNetworkModule(this))
	{
		NetworkModule->ServerExecuteActorFuncOneParam(InActor, InFuncName, InParam);
	}
}

bool UNetworkModuleNetworkComponent::ServerExecuteActorFuncTwoParam_Validate(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2) { return true; }
void UNetworkModuleNetworkComponent::ServerExecuteActorFuncTwoParam_Implementation(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2)
{
	if(ANetworkModule* NetworkModule = UNetworkModuleBPLibrary::GetNetworkModule(this))
	{
		NetworkModule->ServerExecuteActorFuncTwoParam(InActor, InFuncName, InParam1, InParam2);
	}
}

bool UNetworkModuleNetworkComponent::ServerExecuteActorFuncThreeParam_Validate(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3) { return true; }
void UNetworkModuleNetworkComponent::ServerExecuteActorFuncThreeParam_Implementation(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3)
{
	if(ANetworkModule* NetworkModule = UNetworkModuleBPLibrary::GetNetworkModule(this))
	{
		NetworkModule->ServerExecuteActorFuncThreeParam(InActor, InFuncName, InParam1, InParam2, InParam3);
	}
}

bool UNetworkModuleNetworkComponent::ServerExecuteActorFuncFourParam_Validate(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3, FParameter InParam4) { return true; }
void UNetworkModuleNetworkComponent::ServerExecuteActorFuncFourParam_Implementation(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3, FParameter InParam4)
{
	if(ANetworkModule* NetworkModule = UNetworkModuleBPLibrary::GetNetworkModule(this))
	{
		NetworkModule->ServerExecuteActorFuncFourParam(InActor, InFuncName, InParam1, InParam2, InParam3, InParam4);
	}
}

bool UNetworkModuleNetworkComponent::ServerExecuteActorFuncFiveParam_Validate(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3, FParameter InParam4, FParameter InParam5) { return true; }
void UNetworkModuleNetworkComponent::ServerExecuteActorFuncFiveParam_Implementation(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3, FParameter InParam4, FParameter InParam5)
{
	if(ANetworkModule* NetworkModule = UNetworkModuleBPLibrary::GetNetworkModule(this))
	{
		NetworkModule->ServerExecuteActorFuncFiveParam(InActor, InFuncName, InParam1, InParam2, InParam3, InParam4, InParam5);
	}
}