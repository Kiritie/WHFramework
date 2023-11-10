// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/EventModule.h"

#include "Main/MainModule.h"
#include "Debug/DebugModuleTypes.h"
#include "Event/EventModuleStatics.h"
#include "Event/EventModuleNetworkComponent.h"
#include "Event/Manager/EventManagerBase.h"
#include "Event/Handle/EventHandleBase.h"
#include "Common/CommonStatics.h"
#include "Event/Handle/Common/EventHandle_InitGame.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "Net/UnrealNetwork.h"

IMPLEMENTATION_MODULE(UEventModule)

// ParamSets default values
UEventModule::UEventModule()
{
	ModuleName = FName("EventModule");
	ModuleDisplayName = FText::FromString(TEXT("Event Module"));

	ModuleNetworkComponent = UEventModuleNetworkComponent::StaticClass();

	EventManagerClass = UEventManagerBase::StaticClass();
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
	
	if(EventManagerClass)
	{
		EventManager = UObjectPoolModuleStatics::SpawnObject<UEventManagerBase>(nullptr, EventManagerClass);
		EventManager->OnInitialize();
	}
	
	BroadcastEvent<UEventHandle_InitGame>(EEventNetType::Single, this);
}

void UEventModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);

	if(PHASEC(InPhase, EPhase::Final))
	{
		BroadcastEvent<UEventHandle_StartGame>(EEventNetType::Single, this);
	}
}

void UEventModule::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
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

	if(PHASEC(InPhase, EPhase::Final))
	{
		BroadcastEvent<UEventHandle_ExitGame>(EEventNetType::Single, this);
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

void UEventModule::BroadcastEvent(TSubclassOf<UEventHandleBase> InClass, EEventNetType InNetType, UObject* InSender, const TArray<FParameter>& InParams)
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

void UEventModule::MultiBroadcastEvent_Implementation(TSubclassOf<UEventHandleBase> InClass, UObject* InSender, const TArray<FParameter>& InParams)
{
	BroadcastEvent(InClass, EEventNetType::Single, InSender, InParams);
}

void UEventModule::ExecuteEvent(TSubclassOf<UEventHandleBase> InClass, UObject* InSender, const TArray<FParameter>& InParams)
{
	if(!EventHandleInfos.Contains(InClass)) return;
	
	if(UEventHandleBase* EventHandle = UObjectPoolModuleStatics::SpawnObject<UEventHandleBase>(nullptr, InClass))
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

void UEventModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(UEventModule, EventManager);
}
