// Fill out your copyright notice in the Description page of Project Settings.


#include "Network/NetworkModuleBPLibrary.h"

#include "Kismet/GameplayStatics.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"
#include "Network/NetworkModule.h"
#include "Network/NetworkModuleNetworkComponent.h"

void UNetworkModuleBPLibrary::ServerExecuteActorFunc(AActor* InActor, FName InFuncName)
{
	if (!InActor || InFuncName.IsNone()) return;
	
	if (InActor->GetWorld()->GetNetMode() != ENetMode::NM_Client)
	{
		if (UGlobalBPLibrary::ExecuteObjectFunc(InActor, InFuncName, nullptr))
		{
			WH_LOG(WHNetwork, Log, TEXT("ServerExcuteActorFunc : InFuncName : %s, ExecuteActor : %s"), *InFuncName.ToString(), *InActor->GetClass()->GetName());
		}
	}
	else if(UNetworkModuleNetworkComponent* NetworkModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UNetworkModuleNetworkComponent>())
	{
		NetworkModuleNetworkComponent->ServerExecuteActorFunc(InActor, InFuncName);
	}
}

void UNetworkModuleBPLibrary::ServerExecuteActorFuncOneParam(AActor* InActor, FName InFuncName, FParameter InParam)
{
	if (!InActor || InFuncName.IsNone()) return;
	
	if (InActor->GetWorld()->GetNetMode() != ENetMode::NM_Client)
	{
		if (UGlobalBPLibrary::ExecuteObjectFunc(InActor, InFuncName, &InParam))
		{
			WH_LOG(WHNetwork, Log, TEXT("ServerExcuteActorFunc : InFuncName : %s, ExecuteActor : %s"), *InFuncName.ToString(), *InActor->GetClass()->GetName());
		}
	}
	else if(UNetworkModuleNetworkComponent* NetworkModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UNetworkModuleNetworkComponent>())
	{
		NetworkModuleNetworkComponent->ServerExecuteActorFuncOneParam(InActor, InFuncName, InParam);
	}
}

void UNetworkModuleBPLibrary::ServerExecuteActorFuncTwoParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2)
{
	if (!InActor || InFuncName.IsNone()) return;
	
	if (InActor->GetWorld()->GetNetMode() != ENetMode::NM_Client)
	{
		struct
		{
			FParameter Param1;
			FParameter Param2;
		} Params{InParam1, InParam2};
		
		if (UGlobalBPLibrary::ExecuteObjectFunc(InActor, InFuncName, &Params))
		{
			WH_LOG(WHNetwork, Log, TEXT("ServerExcuteActorFunc : InFuncName : %s, ExecuteActor : %s"), *InFuncName.ToString(), *InActor->GetClass()->GetName());
		}
	}
	else if(UNetworkModuleNetworkComponent* NetworkModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UNetworkModuleNetworkComponent>())
	{
		NetworkModuleNetworkComponent->ServerExecuteActorFuncTwoParam(InActor, InFuncName, InParam1, InParam2);
	}
}

void UNetworkModuleBPLibrary::ServerExecuteActorFuncThreeParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3)
{
	if (!InActor || InFuncName.IsNone()) return;
	
	if (InActor->GetWorld()->GetNetMode() != ENetMode::NM_Client)
	{
		struct
		{
			FParameter Param1;
			FParameter Param2;
			FParameter Param3;
		} Params{InParam1, InParam2, InParam3};
		
		if (UGlobalBPLibrary::ExecuteObjectFunc(InActor, InFuncName, &Params))
		{
			WH_LOG(WHNetwork, Log, TEXT("ServerExcuteActorFunc : InFuncName : %s, ExecuteActor : %s"), *InFuncName.ToString(), *InActor->GetClass()->GetName());
		}
	}
	else if(UNetworkModuleNetworkComponent* NetworkModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UNetworkModuleNetworkComponent>())
	{
		NetworkModuleNetworkComponent->ServerExecuteActorFuncThreeParam(InActor, InFuncName, InParam1, InParam2, InParam3);
	}
}

void UNetworkModuleBPLibrary::ServerExecuteActorFuncFourParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3, FParameter InParam4)
{
	if (!InActor || InFuncName.IsNone()) return;
	
	if (InActor->GetWorld()->GetNetMode() != ENetMode::NM_Client)
	{
		struct
		{
			FParameter Param1;
			FParameter Param2;
			FParameter Param3;
			FParameter Param4;
		} Params{InParam1, InParam2, InParam3, InParam4};

		if (UGlobalBPLibrary::ExecuteObjectFunc(InActor, InFuncName, &Params))
		{
			WH_LOG(WHNetwork, Log, TEXT("ServerExcuteActorFunc : InFuncName : %s, ExecuteActor : %s"), *InFuncName.ToString(), *InActor->GetClass()->GetName());
		}
	}
	else if(UNetworkModuleNetworkComponent* NetworkModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UNetworkModuleNetworkComponent>())
	{
		NetworkModuleNetworkComponent->ServerExecuteActorFuncFourParam(InActor, InFuncName, InParam1, InParam2, InParam3, InParam4);
	}
}

void UNetworkModuleBPLibrary::ServerExecuteActorFuncFiveParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3, FParameter InParam4, FParameter InParam5)
{
	if (!InActor || InFuncName.IsNone()) return;
	
	if (InActor->GetWorld()->GetNetMode() != ENetMode::NM_Client)
	{
		struct
		{
			FParameter Param1;
			FParameter Param2;
			FParameter Param3;
			FParameter Param4;
			FParameter Param5;
		} Params{InParam1, InParam2, InParam3, InParam4, InParam5};
		
		if (UGlobalBPLibrary::ExecuteObjectFunc(InActor, InFuncName, &Params))
		{
			WH_LOG(WHNetwork, Log, TEXT("ServerExcuteActorFunc : InFuncName : %s, ExecuteActor : %s"), *InFuncName.ToString(), *InActor->GetClass()->GetName());
		}
	}
	else if(UNetworkModuleNetworkComponent* NetworkModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UNetworkModuleNetworkComponent>())
	{
		NetworkModuleNetworkComponent->ServerExecuteActorFuncFiveParam(InActor, InFuncName, InParam1, InParam2, InParam3, InParam4, InParam5);
	}
}
