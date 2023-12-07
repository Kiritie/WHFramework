// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/EventModule.h"

#include "Main/MainModule.h"
#include "Debug/DebugModuleTypes.h"
#include "Event/EventModuleStatics.h"
#include "Event/EventModuleNetworkComponent.h"
#include "Event/Manager/EventManagerBase.h"
#include "Event/Handle/EventHandleBase.h"
#include "Common/CommonStatics.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "Net/UnrealNetwork.h"

IMPLEMENTATION_MODULE(UEventModule)

// ParamSets default values
UEventModule::UEventModule()
{
	ModuleName = FName("EventModule");
	ModuleDisplayName = FText::FromString(TEXT("Event Module"));

	ModuleNetworkComponent = UEventModuleNetworkComponent::StaticClass();

	EventManagers = TArray<TSubclassOf<UEventManagerBase>>();
	EventManagerRefs = TMap<TSubclassOf<UEventManagerBase>, UEventManagerBase*>();
}

UEventModule::~UEventModule()
{
	TERMINATION_MODULE(UEventModule)
}

#if WITH_EDITOR
void UEventModule::OnGenerate()
{
	Super::OnGenerate();
}

void UEventModule::OnDestroy()
{
	Super::OnDestroy();

	TERMINATION_MODULE(UEventModule)
}
#endif

void UEventModule::OnInitialize()
{
	Super::OnInitialize();
	
	for(auto Iter : EventManagers)
	{
		if(UEventManagerBase* EventManager = UObjectPoolModuleStatics::SpawnObject<UEventManagerBase>(nullptr, nullptr, Iter))
		{
			EventManager->OnInitialize();
			EventManagerRefs.Add(Iter, EventManager);
		}
	}
}

void UEventModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);

	for(auto Iter : EventManagerRefs)
	{
		Iter.Value->OnPreparatory(InPhase);
	}
}

void UEventModule::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);

	for(auto Iter : EventManagerRefs)
	{
		Iter.Value->OnRefresh(DeltaSeconds);
	}
}

void UEventModule::OnPause()
{
	Super::OnPause();
}

void UEventModule::OnUnPause()
{
	Super::OnUnPause();
}

void UEventModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);
	
	for(auto Iter : EventManagerRefs)
	{
		Iter.Value->OnTermination(InPhase);
	}
}

void UEventModule::SubscribeEvent(TSubclassOf<UEventHandleBase> InClass, UObject* InOwner, const FName InFuncName)
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

void UEventModule::SubscribeEvent(TSubclassOf<UEventHandleBase> InClass, const FEventExecuteDelegate& InDelegate)
{
	SubscribeEvent(InClass, InDelegate.GetUObject(), InDelegate.TryGetBoundFunctionName());
}

void UEventModule::SubscribeEventByDelegate(TSubclassOf<UEventHandleBase> InClass, const FEventExecuteDynamicDelegate& InDelegate)
{
	SubscribeEvent(InClass, const_cast<UObject*>(InDelegate.GetUObject()), InDelegate.GetFunctionName());
}

void UEventModule::UnsubscribeEvent(TSubclassOf<UEventHandleBase> InClass, UObject* InOwner, const FName InFuncName)
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

void UEventModule::UnsubscribeEvent(TSubclassOf<UEventHandleBase> InClass, const FEventExecuteDelegate& InDelegate)
{
	UnsubscribeEvent(InClass, InDelegate.GetUObject(), InDelegate.TryGetBoundFunctionName());
}

void UEventModule::UnsubscribeEventByDelegate(TSubclassOf<UEventHandleBase> InClass, const FEventExecuteDynamicDelegate& InDelegate)
{
	UnsubscribeEvent(InClass, const_cast<UObject*>(InDelegate.GetUObject()), InDelegate.GetFunctionName());
}

void UEventModule::UnsubscribeAllEvent()
{
	EventHandleInfos.Empty();
}

void UEventModule::BroadcastEvent(TSubclassOf<UEventHandleBase> InClass, UObject* InSender, const TArray<FParameter>& InParams, EEventNetType InNetType)
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
			if(UEventModuleNetworkComponent* EventModuleNetworkComponent = GetModuleNetworkComponent<UEventModuleNetworkComponent>())
			{
				EventModuleNetworkComponent->ClientBroadcastEvent(InSender, InClass, InParams);
				return;
			}
		}
		case EEventNetType::Server:
		{
			if(UEventModuleNetworkComponent* EventModuleNetworkComponent = GetModuleNetworkComponent<UEventModuleNetworkComponent>())
			{
				EventModuleNetworkComponent->ServerBroadcastEvent(InSender, InClass, InParams);
				return;
			}
		}
		case EEventNetType::Multicast:
		{
			if(UEventModuleNetworkComponent* EventModuleNetworkComponent = GetModuleNetworkComponent<UEventModuleNetworkComponent>())
			{
				EventModuleNetworkComponent->ServerBroadcastEventMulticast(InSender, InClass, InParams);
				return;
			}
		}
		default: break;
	}
	
	EventHandleInfos[InClass].EventHandleDelegate.ExecuteIfBound(InClass, InSender, InParams);
}

void UEventModule::BroadcastEventByHandle(UEventHandleBase* InHandle, UObject* InSender, EEventNetType InNetType)
{
	BroadcastEvent(InHandle->GetClass(), InSender, InHandle->Pack(), InNetType);
}

void UEventModule::MultiBroadcastEvent_Implementation(TSubclassOf<UEventHandleBase> InClass, UObject* InSender, const TArray<FParameter>& InParams)
{
	BroadcastEvent(InClass, InSender, InParams, EEventNetType::Single);
}

void UEventModule::ExecuteEvent(TSubclassOf<UEventHandleBase> InClass, UObject* InSender, const TArray<FParameter>& InParams)
{
	if(!EventHandleInfos.Contains(InClass)) return;
	
	if(UEventHandleBase* EventHandle = UObjectPoolModuleStatics::SpawnObject<UEventHandleBase>(nullptr, nullptr, InClass))
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
				if (EventHandle->Filter(Iter1.Key, Iter2) && UCommonStatics::ExecuteObjectFunc(Iter1.Key, Iter2, &Params))
				{
					WHLog(FString::Printf(TEXT("ExecuteEvent : FuncName : %s, EventOwner : %s"), *Iter2.ToString(), *Iter1.Key->GetClass()->GetName()), EDC_Event);
				}
			}
		}

		UObjectPoolModuleStatics::DespawnObject(EventHandle);
	}
}

UEventManagerBase* UEventModule::GetEventManager(TSubclassOf<UEventManagerBase> InClass) const
{
	if(EventManagerRefs.Contains(InClass))
	{
		return EventManagerRefs[InClass];
	}
	return nullptr;
}

void UEventModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(UEventModule, EventManagerRefs);
}
