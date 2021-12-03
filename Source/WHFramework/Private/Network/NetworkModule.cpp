// Fill out your copyright notice in the Description page of Project Settings.


#include "Network/NetworkModule.h"

#include "Main/MainModule.h"
#include "Debug/DebugModuleTypes.h"
#include "Network/NetworkModuleBPLibrary.h"
#include "Network/NetworkModuleNetworkComponent.h"

// ParamSets default values
ANetworkModule::ANetworkModule()
{
	ModuleName = FName("NetworkModule");
}

#if WITH_EDITOR
void ANetworkModule::OnGenerate_Implementation()
{
	Super::OnGenerate_Implementation();
}

void ANetworkModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
}
#endif

void ANetworkModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void ANetworkModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void ANetworkModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void ANetworkModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

bool ANetworkModule::ExecuteObjectFunc(UObject* InObject, const FName& InFuncName, void* Params)
{
	if (ensureEditor(InObject))
	{
		UFunction* Func = InObject->FindFunction(InFuncName);
		if (ensureEditorMsgf(Func, TEXT("错误的函数调用, %s 对应函数名称 : %s ,并不存在"), *InObject->GetName(), *InFuncName.ToString()))
		{
			InObject->ProcessEvent(Func, Params);
			return true;
		}
	}

	return false;
}

void ANetworkModule::ServerExecuteActorFunc(AActor* InActor, FName InFuncName)
{
	if (GetWorld()->GetNetMode() != ENetMode::NM_Client)
	{
		if (ExecuteObjectFunc(InActor, InFuncName, nullptr))
		{
			WH_LOG(WHNetwork, Log, TEXT("ServerExcuteActorFunc : InFuncName : %s, ExecuteActor : %s"), *InFuncName.ToString(), *InActor->GetClass()->GetName());
		}
	}
	else if(UNetworkModuleNetworkComponent* NetworkModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UNetworkModuleNetworkComponent>())
	{
		NetworkModuleNetworkComponent->ServerExecuteActorFunc(InActor, InFuncName);
	}
}

void ANetworkModule::ServerExecuteActorFuncOneParam(AActor* InActor, FName InFuncName, FParameter InParam)
{
	if (GetWorld()->GetNetMode() != ENetMode::NM_Client)
	{
		if (ExecuteObjectFunc(InActor, InFuncName, &InParam))
		{
			WH_LOG(WHNetwork, Log, TEXT("ServerExcuteActorFunc : InFuncName : %s, ExecuteActor : %s"), *InFuncName.ToString(), *InActor->GetClass()->GetName());
		}
	}
	else if(UNetworkModuleNetworkComponent* NetworkModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UNetworkModuleNetworkComponent>())
	{
		NetworkModuleNetworkComponent->ServerExecuteActorFuncOneParam(InActor, InFuncName, InParam);
	}
}

void ANetworkModule::ServerExecuteActorFuncTwoParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2)
{
	if (GetWorld()->GetNetMode() != ENetMode::NM_Client)
	{
		struct
		{
			FParameter Param1;
			FParameter Param2;
		} Params{InParam1, InParam2};
		
		if (ExecuteObjectFunc(InActor, InFuncName, &Params))
		{
			WH_LOG(WHNetwork, Log, TEXT("ServerExcuteActorFunc : InFuncName : %s, ExecuteActor : %s"), *InFuncName.ToString(), *InActor->GetClass()->GetName());
		}
	}
	else if(UNetworkModuleNetworkComponent* NetworkModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UNetworkModuleNetworkComponent>())
	{
		NetworkModuleNetworkComponent->ServerExecuteActorFuncTwoParam(InActor, InFuncName, InParam1, InParam2);
	}
}

void ANetworkModule::ServerExecuteActorFuncThreeParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3)
{
	if (GetWorld()->GetNetMode() != ENetMode::NM_Client)
	{
		struct
		{
			FParameter Param1;
			FParameter Param2;
			FParameter Param3;
		} Params{InParam1, InParam2, InParam3};
		
		if (ExecuteObjectFunc(InActor, InFuncName, &Params))
		{
			WH_LOG(WHNetwork, Log, TEXT("ServerExcuteActorFunc : InFuncName : %s, ExecuteActor : %s"), *InFuncName.ToString(), *InActor->GetClass()->GetName());
		}
	}
	else if(UNetworkModuleNetworkComponent* NetworkModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UNetworkModuleNetworkComponent>())
	{
		NetworkModuleNetworkComponent->ServerExecuteActorFuncThreeParam(InActor, InFuncName, InParam1, InParam2, InParam3);
	}
}

void ANetworkModule::ServerExecuteActorFuncFourParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3, FParameter InParam4)
{
	if (GetWorld()->GetNetMode() != ENetMode::NM_Client)
	{
		struct
		{
			FParameter Param1;
			FParameter Param2;
			FParameter Param3;
			FParameter Param4;
		} Params{InParam1, InParam2, InParam3, InParam4};

		if (ExecuteObjectFunc(InActor, InFuncName, &Params))
		{
			WH_LOG(WHNetwork, Log, TEXT("ServerExcuteActorFunc : InFuncName : %s, ExecuteActor : %s"), *InFuncName.ToString(), *InActor->GetClass()->GetName());
		}
	}
	else if(UNetworkModuleNetworkComponent* NetworkModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UNetworkModuleNetworkComponent>())
	{
		NetworkModuleNetworkComponent->ServerExecuteActorFuncFourParam(InActor, InFuncName, InParam1, InParam2, InParam3, InParam4);
	}
}

void ANetworkModule::ServerExecuteActorFuncFiveParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3, FParameter InParam4, FParameter InParam5)
{
	if (GetWorld()->GetNetMode() != ENetMode::NM_Client)
	{
		struct
		{
			FParameter Param1;
			FParameter Param2;
			FParameter Param3;
			FParameter Param4;
			FParameter Param5;
		} Params{InParam1, InParam2, InParam3, InParam4, InParam5};
		
		if (ExecuteObjectFunc(InActor, InFuncName, &Params))
		{
			WH_LOG(WHNetwork, Log, TEXT("ServerExcuteActorFunc : InFuncName : %s, ExecuteActor : %s"), *InFuncName.ToString(), *InActor->GetClass()->GetName());
		}
	}
	else if(UNetworkModuleNetworkComponent* NetworkModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UNetworkModuleNetworkComponent>())
	{
		NetworkModuleNetworkComponent->ServerExecuteActorFuncFiveParam(InActor, InFuncName, InParam1, InParam2, InParam3, InParam4, InParam5);
	}
}
