// Fill out your copyright notice in the Description page of Project Settings.


#include "Network/NetworkModuleBPLibrary.h"

#include "Kismet/GameplayStatics.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"
#include "Network/NetworkModule.h"

bool UNetworkModuleBPLibrary::ExecuteObjectFunc(UObject* InObject, const FName& InFuncName, void* Params)
{
	if(ANetworkModule* NetworkModule = AMainModule::GetModuleByClass<ANetworkModule>())
	{
		return NetworkModule->ExecuteObjectFunc(InObject, InFuncName, Params);
	}
	return false;
}

void UNetworkModuleBPLibrary::ServerExecuteActorFunc(AActor* InActor, FName InFuncName)
{
	if(ANetworkModule* NetworkModule = AMainModule::GetModuleByClass<ANetworkModule>())
	{
		NetworkModule->ServerExecuteActorFunc(InActor, InFuncName);
	}
}

void UNetworkModuleBPLibrary::ServerExecuteActorFuncOneParam(AActor* InActor, FName InFuncName, FParameter InParam)
{
	if(ANetworkModule* NetworkModule = AMainModule::GetModuleByClass<ANetworkModule>())
	{
		NetworkModule->ServerExecuteActorFuncOneParam(InActor, InFuncName, InParam);
	}
}

void UNetworkModuleBPLibrary::ServerExecuteActorFuncTwoParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2)
{
	if(ANetworkModule* NetworkModule = AMainModule::GetModuleByClass<ANetworkModule>())
	{
		NetworkModule->ServerExecuteActorFuncTwoParam(InActor, InFuncName, InParam1, InParam2);
	}
}

void UNetworkModuleBPLibrary::ServerExecuteActorFuncThreeParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3)
{
	if(ANetworkModule* NetworkModule = AMainModule::GetModuleByClass<ANetworkModule>())
	{
		NetworkModule->ServerExecuteActorFuncThreeParam(InActor, InFuncName, InParam1, InParam2, InParam3);
	}
}

void UNetworkModuleBPLibrary::ServerExecuteActorFuncFourParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3, FParameter InParam4)
{
	if(ANetworkModule* NetworkModule = AMainModule::GetModuleByClass<ANetworkModule>())
	{
		NetworkModule->ServerExecuteActorFuncFourParam(InActor, InFuncName, InParam1, InParam2, InParam3, InParam4);
	}
}

void UNetworkModuleBPLibrary::ServerExecuteActorFuncFiveParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3, FParameter InParam4, FParameter InParam5)
{
	if(ANetworkModule* NetworkModule = AMainModule::GetModuleByClass<ANetworkModule>())
	{
		NetworkModule->ServerExecuteActorFuncFiveParam(InActor, InFuncName, InParam1, InParam2, InParam3, InParam4, InParam5);
	}
}
