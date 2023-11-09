// Fill out your copyright notice in the Description page of Project Settings.


#include "Network/NetworkModuleStatics.h"

#include "Debug/DebugModuleTypes.h"
#include "Common/CommonStatics.h"
#include "Main/MainModule.h"
#include "Network/NetworkModule.h"
#include "Network/NetworkModuleNetworkComponent.h"

bool UNetworkModuleStatics::IsNetworkLocalMode()
{
	if(UNetworkModule* NetworkModule = UNetworkModule::Get())
	{
		return NetworkModule->IsLocalMode();
	}
	return false;
}

void UNetworkModuleStatics::SetNetworkLocalMode(bool bInLocalMode)
{
	if(UNetworkModule* NetworkModule = UNetworkModule::Get())
	{
		NetworkModule->SetLocalMode(bInLocalMode);
	}
}

FString UNetworkModuleStatics::GetNetworkServerURL()
{
	if(UNetworkModule* NetworkModule = UNetworkModule::Get())
	{
		return NetworkModule->GetServerURL();
	}
	return TEXT("");
}

void UNetworkModuleStatics::SetNetworkServerURL(const FString& InServerURL)
{
	if(UNetworkModule* NetworkModule = UNetworkModule::Get())
	{
		NetworkModule->SetServerURL(InServerURL);
	}
}

int32 UNetworkModuleStatics::GetNetworkServerPort()
{
	if(UNetworkModule* NetworkModule = UNetworkModule::Get())
	{
		return NetworkModule->GetServerPort();
	}
	return 0;
}

void UNetworkModuleStatics::SetNetworkServerPort(int32 InServerPort)
{
	if(UNetworkModule* NetworkModule = UNetworkModule::Get())
	{
		NetworkModule->SetServerPort(InServerPort);
	}
}

FString UNetworkModuleStatics::GetServerLocalURL()
{
	if(UNetworkModule* NetworkModule = UNetworkModule::Get())
	{
		return NetworkModule->GetServerLocalURL();
	}
	return TEXT("");
}

FString UNetworkModuleStatics::GetServerLocalIP()
{
	if(UNetworkModule* NetworkModule = UNetworkModule::Get())
	{
		return NetworkModule->GetServerLocalIP();
	}
	return TEXT("");
}

int32 UNetworkModuleStatics::GetServerLocalPort()
{
	if(UNetworkModule* NetworkModule = UNetworkModule::Get())
	{
		return NetworkModule->GetServerLocalPort();
	}
	return 0;
}

bool UNetworkModuleStatics::ConnectNetworkServer(const FString& InServerURL, int32 InServerPort, const FString& InOptions)
{
	if(UNetworkModule* NetworkModule = UNetworkModule::Get())
	{
		return NetworkModule->ConnectServer(InServerURL, InServerPort, InOptions);
	}
	return false;
}

bool UNetworkModuleStatics::DisconnectNetworkServer(const FString& InLevelName)
{
	if(UNetworkModule* NetworkModule = UNetworkModule::Get())
	{
		return NetworkModule->DisconnectServer(InLevelName);
	}
	return false;
}

void UNetworkModuleStatics::ServerExecuteActorFunc(AActor* InActor, FName InFuncName)
{
	if (!InActor || InFuncName.IsNone()) return;
	
	if (InActor->GetWorld()->GetNetMode() != ENetMode::NM_Client)
	{
		if (UCommonStatics::ExecuteObjectFunc(InActor, InFuncName, nullptr))
		{
			WHLog(FString::Printf(TEXT("ServerExcuteActorFunc : InFuncName : %s, ExecuteActor : %s"), *InFuncName.ToString(), *InActor->GetClass()->GetName()), EDC_Network);
		}
	}
	else if(UNetworkModuleNetworkComponent* NetworkModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UNetworkModuleNetworkComponent>())
	{
		NetworkModuleNetworkComponent->ServerExecuteActorFunc(InActor, InFuncName);
	}
}

void UNetworkModuleStatics::ServerExecuteActorFuncOneParam(AActor* InActor, FName InFuncName, FParameter InParam)
{
	if (!InActor || InFuncName.IsNone()) return;
	
	if (InActor->GetWorld()->GetNetMode() != ENetMode::NM_Client)
	{
		if (UCommonStatics::ExecuteObjectFunc(InActor, InFuncName, &InParam))
		{
			WHLog(FString::Printf(TEXT("ServerExcuteActorFunc : InFuncName : %s, ExecuteActor : %s"), *InFuncName.ToString(), *InActor->GetClass()->GetName()), EDC_Network);
		}
	}
	else if(UNetworkModuleNetworkComponent* NetworkModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UNetworkModuleNetworkComponent>())
	{
		NetworkModuleNetworkComponent->ServerExecuteActorFuncOneParam(InActor, InFuncName, InParam);
	}
}

void UNetworkModuleStatics::ServerExecuteActorFuncTwoParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2)
{
	if (!InActor || InFuncName.IsNone()) return;
	
	if (InActor->GetWorld()->GetNetMode() != ENetMode::NM_Client)
	{
		struct
		{
			FParameter Param1;
			FParameter Param2;
		} Params{InParam1, InParam2};
		
		if (UCommonStatics::ExecuteObjectFunc(InActor, InFuncName, &Params))
		{
			WHLog(FString::Printf(TEXT("ServerExcuteActorFunc : InFuncName : %s, ExecuteActor : %s"), *InFuncName.ToString(), *InActor->GetClass()->GetName()), EDC_Network);
		}
	}
	else if(UNetworkModuleNetworkComponent* NetworkModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UNetworkModuleNetworkComponent>())
	{
		NetworkModuleNetworkComponent->ServerExecuteActorFuncTwoParam(InActor, InFuncName, InParam1, InParam2);
	}
}

void UNetworkModuleStatics::ServerExecuteActorFuncThreeParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3)
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
		
		if (UCommonStatics::ExecuteObjectFunc(InActor, InFuncName, &Params))
		{
			WHLog(FString::Printf(TEXT("ServerExcuteActorFunc : InFuncName : %s, ExecuteActor : %s"), *InFuncName.ToString(), *InActor->GetClass()->GetName()), EDC_Network);
		}
	}
	else if(UNetworkModuleNetworkComponent* NetworkModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UNetworkModuleNetworkComponent>())
	{
		NetworkModuleNetworkComponent->ServerExecuteActorFuncThreeParam(InActor, InFuncName, InParam1, InParam2, InParam3);
	}
}

void UNetworkModuleStatics::ServerExecuteActorFuncFourParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3, FParameter InParam4)
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

		if (UCommonStatics::ExecuteObjectFunc(InActor, InFuncName, &Params))
		{
			WHLog(FString::Printf(TEXT("ServerExcuteActorFunc : InFuncName : %s, ExecuteActor : %s"), *InFuncName.ToString(), *InActor->GetClass()->GetName()), EDC_Network);
		}
	}
	else if(UNetworkModuleNetworkComponent* NetworkModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UNetworkModuleNetworkComponent>())
	{
		NetworkModuleNetworkComponent->ServerExecuteActorFuncFourParam(InActor, InFuncName, InParam1, InParam2, InParam3, InParam4);
	}
}

void UNetworkModuleStatics::ServerExecuteActorFuncFiveParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3, FParameter InParam4, FParameter InParam5)
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
		
		if (UCommonStatics::ExecuteObjectFunc(InActor, InFuncName, &Params))
		{
			WHLog(FString::Printf(TEXT("ServerExcuteActorFunc : InFuncName : %s, ExecuteActor : %s"), *InFuncName.ToString(), *InActor->GetClass()->GetName()), EDC_Network);
		}
	}
	else if(UNetworkModuleNetworkComponent* NetworkModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UNetworkModuleNetworkComponent>())
	{
		NetworkModuleNetworkComponent->ServerExecuteActorFuncFiveParam(InActor, InFuncName, InParam1, InParam2, InParam3, InParam4, InParam5);
	}
}
