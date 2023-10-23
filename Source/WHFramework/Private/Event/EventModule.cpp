// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/EventModule.h"

#include "Main/MainModule.h"
#include "Debug/DebugModuleTypes.h"
#include "Event/EventModuleBPLibrary.h"
#include "Event/EventModuleNetworkComponent.h"
#include "Event/Manager/EventManagerBase.h"
#include "Event/Handle/EventHandleBase.h"
#include "Common/CommonBPLibrary.h"
#include "ObjectPool/ObjectPoolModuleBPLibrary.h"
#include "Net/UnrealNetwork.h"

IMPLEMENTATION_MODULE(AEventModule)

// ParamSets default values
AEventModule::AEventModule()
{
	ModuleName = FName("EventModule");
	
	EventManagerClass = UEventManagerBase::StaticClass();
}

AEventModule::~AEventModule()
{
	TERMINATION_MODULE(AEventModule)
}

#if WITH_EDITOR
void AEventModule::OnGenerate()
{
	Super::OnGenerate();
}

void AEventModule::OnDestroy()
{
	Super::OnDestroy();
}
#endif

void AEventModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
	
	if(EventManagerClass)
	{
		EventManager = UObjectPoolModuleBPLibrary::SpawnObject<UEventManagerBase>(nullptr, EventManagerClass);
		EventManager->OnInitialize();
	}
}

void AEventModule::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);

	if(PHASEC(InPhase, EPhase::Final))
	{
		BroadcastEvent<UEventHandle_StartGame>(EEventNetType::Single, this);
	}
}

void AEventModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void AEventModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void AEventModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

void AEventModule::OnTermination_Implementation(EPhase InPhase)
{
	Super::OnTermination_Implementation(InPhase);

	if(PHASEC(InPhase, EPhase::Final))
	{
		BroadcastEvent<UEventHandle_ExitGame>(EEventNetType::Single, this);
	}
}

void AEventModule::SubscribeEvent(TSubclassOf<UEventHandleBase> InClass, UObject* InOwner, const FName InFuncName)
{
	if(!InClass || !InOwner || InFuncName.IsNone()) return;

	if(!EventHandleInfos.Contains(InClass))
	{
		EventHandleInfos.Add(InClass, FEventHandleInfo(FEventHandleDelegate()));
	}

	if(!EventHandleInfos[InClass].EventHandleDelegate.IsBoundToObject(this))
	{
		EventHandleInfos[InClass].EventHandleDelegate.BindUFunction(this, FName("ExecuteEvent"));
	}

	switch (InClass->GetDefaultObject<UEventHandleBase>()->EventType)
	{
		case EEventType::Single:
		{
			EventHandleInfos[InClass].EventHandleFuncMap.Empty();
		}
		case EEventType::Multicast:
		{
			if(!EventHandleInfos[InClass].EventHandleFuncMap.Contains(InOwner))
			{
				EventHandleInfos[InClass].EventHandleFuncMap.Add(InOwner, FEventHandleFuncs());
			}
			EventHandleInfos[InClass].EventHandleFuncMap[InOwner].FuncNames.Add(InFuncName);
			break;
		}
	}
}

void AEventModule::SubscribeEvent(TSubclassOf<UEventHandleBase> InClass, const FEventExecuteDelegate& InDelegate)
{
	SubscribeEvent(InClass, InDelegate.GetUObject(), InDelegate.TryGetBoundFunctionName());
}

void AEventModule::SubscribeEventByDelegate(TSubclassOf<UEventHandleBase> InClass, const FEventExecuteDynamicDelegate& InDelegate)
{
	SubscribeEvent(InClass, const_cast<UObject*>(InDelegate.GetUObject()), InDelegate.GetFunctionName());
}

void AEventModule::UnsubscribeEvent(TSubclassOf<UEventHandleBase> InClass, UObject* InOwner, const FName InFuncName)
{
	if(!InClass || !InOwner || InFuncName.IsNone()) return;

	if(!EventHandleInfos.Contains(InClass))
	{
		EventHandleInfos.Add(InClass, FEventHandleInfo(FEventHandleDelegate()));
	}
	
	if(EventHandleInfos[InClass].EventHandleFuncMap.Contains(InOwner))
	{
		if(EventHandleInfos[InClass].EventHandleFuncMap[InOwner].FuncNames.Contains(InFuncName))
		{
			EventHandleInfos[InClass].EventHandleFuncMap[InOwner].FuncNames.Remove(InFuncName);
		}
		if(EventHandleInfos[InClass].EventHandleFuncMap[InOwner].FuncNames.Num() == 0)
		{
			EventHandleInfos[InClass].EventHandleFuncMap.Remove(InOwner);
		}
	}

	if(EventHandleInfos[InClass].EventHandleFuncMap.Num() == 0)
	{
		EventHandleInfos[InClass].EventHandleDelegate.Unbind();
	}
}

void AEventModule::UnsubscribeEvent(TSubclassOf<UEventHandleBase> InClass, const FEventExecuteDelegate& InDelegate)
{
	UnsubscribeEvent(InClass, InDelegate.GetUObject(), InDelegate.TryGetBoundFunctionName());
}

void AEventModule::UnsubscribeEventByDelegate(TSubclassOf<UEventHandleBase> InClass, const FEventExecuteDynamicDelegate& InDelegate)
{
	UnsubscribeEvent(InClass, const_cast<UObject*>(InDelegate.GetUObject()), InDelegate.GetFunctionName());
}

void AEventModule::UnsubscribeAllEvent()
{
	EventHandleInfos.Empty();
}

void AEventModule::BroadcastEvent(TSubclassOf<UEventHandleBase> InClass, EEventNetType InNetType, UObject* InSender, const TArray<FParameter>& InParams)
{
	if(!InClass) return;

	if(!EventHandleInfos.Contains(InClass))
	{
		EventHandleInfos.Add(InClass, FEventHandleInfo(FEventHandleDelegate()));
	}

	switch(InNetType)
	{
		case EEventNetType::Client:
		{
			if(UEventModuleNetworkComponent* EventModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UEventModuleNetworkComponent>())
			{
				EventModuleNetworkComponent->ClientBroadcastEvent(InSender, InClass, InParams);
				return;
			}
		}
		case EEventNetType::Server:
		{
			if(UEventModuleNetworkComponent* EventModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UEventModuleNetworkComponent>())
			{
				EventModuleNetworkComponent->ServerBroadcastEvent(InSender, InClass, InParams);
				return;
			}
		}
		case EEventNetType::Multicast:
		{
			if(UEventModuleNetworkComponent* EventModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UEventModuleNetworkComponent>())
			{
				EventModuleNetworkComponent->ServerBroadcastEventMulticast(InSender, InClass, InParams);
				return;
			}
		}
		default: break;
	}
	
	EventHandleInfos[InClass].EventHandleDelegate.ExecuteIfBound(InClass, InSender, InParams);
}

void AEventModule::MultiBroadcastEvent_Implementation(TSubclassOf<UEventHandleBase> InClass, UObject* InSender, const TArray<FParameter>& InParams)
{
	BroadcastEvent(InClass, EEventNetType::Single, InSender, InParams);
}

void AEventModule::ExecuteEvent(TSubclassOf<UEventHandleBase> InClass, UObject* InSender, const TArray<FParameter>& InParams)
{
	if(!EventHandleInfos.Contains(InClass)) return;
	
	if(UEventHandleBase* EventHandle = UObjectPoolModuleBPLibrary::SpawnObject<UEventHandleBase>(nullptr, InClass))
	{
		EventHandle->Fill(InParams);
		
		struct
		{
			UObject* Sender;
			UEventHandleBase* EventHandle;
		} Params{InSender, EventHandle};

		auto TmpEventHandleInfos = EventHandleInfos;
		for (auto Iter1 : TmpEventHandleInfos[InClass].EventHandleFuncMap)
		{
			for (auto Iter2 : Iter1.Value.FuncNames)
			{
				if (EventHandle->Filter(Iter1.Key, Iter2) && UCommonBPLibrary::ExecuteObjectFunc(Iter1.Key, Iter2, &Params))
				{
					WHLog(FString::Printf(TEXT("ExecuteEvent : FuncName : %s, EventOwner : %s"), *Iter2.ToString(), *Iter1.Key->GetClass()->GetName()), EDC_Event);
				}
			}
		}

		UObjectPoolModuleBPLibrary::DespawnObject(EventHandle);
	}
}

void AEventModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AEventModule, EventManager);
}
