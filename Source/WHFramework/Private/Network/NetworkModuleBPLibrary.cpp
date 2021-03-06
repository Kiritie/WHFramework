// Fill out your copyright notice in the Description page of Project Settings.


#include "Network/NetworkModuleBPLibrary.h"

#include "Global/GlobalBPLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"
#include "Network/NetworkModule.h"
#include "Network/NetworkModuleNetworkComponent.h"

bool UNetworkModuleBPLibrary::IsNetworkLocalMode()
{
	if(ANetworkModule* NetworkModule = AMainModule::GetModuleByClass<ANetworkModule>())
	{
		return NetworkModule->IsLocalMode();
	}
	return false;
}

void UNetworkModuleBPLibrary::SetNetworkLocalMode(bool bInLocalMode)
{
	if(ANetworkModule* NetworkModule = AMainModule::GetModuleByClass<ANetworkModule>())
	{
		NetworkModule->SetLocalMode(bInLocalMode);
	}
}

FString UNetworkModuleBPLibrary::GetNetworkServerURL()
{
	if(ANetworkModule* NetworkModule = AMainModule::GetModuleByClass<ANetworkModule>())
	{
		return NetworkModule->GetServerURL();
	}
	return TEXT("");
}

void UNetworkModuleBPLibrary::SetNetworkServerURL(const FString& InServerURL)
{
	if(ANetworkModule* NetworkModule = AMainModule::GetModuleByClass<ANetworkModule>())
	{
		NetworkModule->SetServerURL(InServerURL);
	}
}

int32 UNetworkModuleBPLibrary::GetNetworkServerPort()
{
	if(ANetworkModule* NetworkModule = AMainModule::GetModuleByClass<ANetworkModule>())
	{
		return NetworkModule->GetServerPort();
	}
	return 0;
}

void UNetworkModuleBPLibrary::SetNetworkServerPort(int32 InServerPort)
{
	if(ANetworkModule* NetworkModule = AMainModule::GetModuleByClass<ANetworkModule>())
	{
		NetworkModule->SetServerPort(InServerPort);
	}
}

FString UNetworkModuleBPLibrary::GetServerLocalURL()
{
	if(ANetworkModule* NetworkModule = AMainModule::GetModuleByClass<ANetworkModule>())
	{
		return NetworkModule->GetServerLocalURL();
	}
	return TEXT("");
}

FString UNetworkModuleBPLibrary::GetServerLocalIP()
{
	if(ANetworkModule* NetworkModule = AMainModule::GetModuleByClass<ANetworkModule>())
	{
		return NetworkModule->GetServerLocalIP();
	}
	return TEXT("");
}

int32 UNetworkModuleBPLibrary::GetServerLocalPort()
{
	if(ANetworkModule* NetworkModule = AMainModule::GetModuleByClass<ANetworkModule>())
	{
		return NetworkModule->GetServerLocalPort();
	}
	return 0;
}

bool UNetworkModuleBPLibrary::ConnectNetworkServer(const FString& InServerURL, int32 InServerPort, const FString& InOptions)
{
	if(ANetworkModule* NetworkModule = AMainModule::GetModuleByClass<ANetworkModule>())
	{
		return NetworkModule->ConnectServer(InServerURL, InServerPort, InOptions);
	}
	return false;
}

bool UNetworkModuleBPLibrary::DisconnectNetworkServer(const FString& InLevelName)
{
	if(ANetworkModule* NetworkModule = AMainModule::GetModuleByClass<ANetworkModule>())
	{
		return NetworkModule->DisconnectServer(InLevelName);
	}
	return false;
}

void UNetworkModuleBPLibrary::ServerExecuteActorFunc(AActor* InActor, FName InFuncName)
{
	if (!InActor || InFuncName.IsNone()) return;
	
	if (InActor->GetWorld()->GetNetMode() != ENetMode::NM_Client)
	{
		if (UGlobalBPLibrary::ExecuteObjectFunc(InActor, InFuncName, nullptr))
		{
			WHLog(WH_Network, Log, TEXT("ServerExcuteActorFunc : InFuncName : %s, ExecuteActor : %s"), *InFuncName.ToString(), *InActor->GetClass()->GetName());
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
			WHLog(WH_Network, Log, TEXT("ServerExcuteActorFunc : InFuncName : %s, ExecuteActor : %s"), *InFuncName.ToString(), *InActor->GetClass()->GetName());
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
			WHLog(WH_Network, Log, TEXT("ServerExcuteActorFunc : InFuncName : %s, ExecuteActor : %s"), *InFuncName.ToString(), *InActor->GetClass()->GetName());
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
			WHLog(WH_Network, Log, TEXT("ServerExcuteActorFunc : InFuncName : %s, ExecuteActor : %s"), *InFuncName.ToString(), *InActor->GetClass()->GetName());
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
			WHLog(WH_Network, Log, TEXT("ServerExcuteActorFunc : InFuncName : %s, ExecuteActor : %s"), *InFuncName.ToString(), *InActor->GetClass()->GetName());
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
			WHLog(WH_Network, Log, TEXT("ServerExcuteActorFunc : InFuncName : %s, ExecuteActor : %s"), *InFuncName.ToString(), *InActor->GetClass()->GetName());
		}
	}
	else if(UNetworkModuleNetworkComponent* NetworkModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UNetworkModuleNetworkComponent>())
	{
		NetworkModuleNetworkComponent->ServerExecuteActorFuncFiveParam(InActor, InFuncName, InParam1, InParam2, InParam3, InParam4, InParam5);
	}
}
