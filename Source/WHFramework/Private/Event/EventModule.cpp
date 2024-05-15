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

IMPLEMENTATION_MODULE(UEventModule)

// ParamSets default values
UEventModule::UEventModule()
{
	ModuleName = FName("EventModule");
	ModuleDisplayName = FText::FromString(TEXT("Event Module"));

	bModuleRequired = true;

	ModuleNetworkComponent = UEventModuleNetworkComponent::StaticClass();

	EventManagers = TArray<TSubclassOf<UEventManagerBase>>();
	EventManagerRefs = TMap<FName, UEventManagerBase*>();
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
		if(UEventManagerBase* EventManager = UObjectPoolModuleStatics::SpawnObject<UEventManagerBase>(nullptr, nullptr, false, Iter))
		{
			EventManager->OnInitialize();
			EventManagerRefs.Add(EventManager->GetEventManagerName(), EventManager);
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

void UEventModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);

	if(bInEditor) return;

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

	FEventMapping& Mapping = EventMappings.FindOrAdd(InClass);

	if(!Mapping.Delegate.IsBoundToObject(this))
	{
		Mapping.Delegate.BindUFunction(this, FName("ExecuteEvent"));
	}

	switch (InClass->GetDefaultObject<UEventHandleBase>()->EventType)
	{
		case EEventType::Single:
		{
			Mapping.FuncMap.Empty();
		}
		case EEventType::Multicast:
		{
			if(!Mapping.FuncMap.Contains(InOwner))
			{
				Mapping.FuncMap.Add(InOwner, FEventFuncs());
			}
			Mapping.FuncMap[InOwner].FuncNames.Add(InFuncName);
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

	FEventMapping& Mapping = EventMappings.FindOrAdd(InClass);
	
	if(Mapping.FuncMap.Contains(InOwner))
	{
		if(Mapping.FuncMap[InOwner].FuncNames.Contains(InFuncName))
		{
			Mapping.FuncMap[InOwner].FuncNames.Remove(InFuncName);
		}
		if(Mapping.FuncMap[InOwner].FuncNames.Num() == 0)
		{
			Mapping.FuncMap.Remove(InOwner);
		}
	}

	if(Mapping.FuncMap.Num() == 0)
	{
		Mapping.Delegate.Unbind();
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
	EventMappings.Empty();
}

void UEventModule::BroadcastEvent(TSubclassOf<UEventHandleBase> InClass, UObject* InSender, const TArray<FParameter>& InParams, EEventNetType InNetType, bool bRecovery)
{
	if(!InClass) return;

	const FEventMapping& Mapping = EventMappings.FindOrAdd(InClass);

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
				EventModuleNetworkComponent->ServerBroadcastEventMulticast(InSender, InClass, InParams, bRecovery);
				return;
			}
		}
		default: break;
	}
	
	Mapping.Delegate.ExecuteIfBound(InClass, InSender, InParams, bRecovery);
}

void UEventModule::BroadcastEventByHandle(UEventHandleBase* InHandle, UObject* InSender, EEventNetType InNetType, bool bRecovery)
{
	BroadcastEvent(InHandle->GetClass(), InSender, InHandle->Pack(), InNetType, bRecovery);
}

void UEventModule::MultiBroadcastEvent_Implementation(TSubclassOf<UEventHandleBase> InClass, UObject* InSender, const TArray<FParameter>& InParams, bool bRecovery)
{
	BroadcastEvent(InClass, InSender, InParams, EEventNetType::Single, bRecovery);
}

void UEventModule::ExecuteEvent(TSubclassOf<UEventHandleBase> InClass, UObject* InSender, const TArray<FParameter>& InParams, bool bRecovery)
{
	if(!EventMappings.Contains(InClass)) return;
	
	if(UEventHandleBase* EventHandle = UObjectPoolModuleStatics::SpawnObject<UEventHandleBase>(nullptr, nullptr, false, InClass))
	{
		EventHandle->Parse(InParams);
		
		struct
		{
			UObject* Sender;
			UEventHandleBase* EventHandle;
		} Params{InSender, EventHandle};

		FEventMapping Mapping = EventMappings[InClass];
		for (auto& Iter1 : Mapping.FuncMap)
		{
			for (auto Iter2 : Iter1.Value.FuncNames)
			{
				if (EventHandle->Filter(Iter1.Key, Iter2) && UCommonStatics::ExecuteObjectFunc(Iter1.Key, Iter2, &Params))
				{
					WHLog(FString::Printf(TEXT("ExecuteEvent : FuncName : %s, EventOwner : %s"), *Iter2.ToString(), *Iter1.Key->GetClass()->GetName()), EDC_Event);
				}
			}
		}

		if(bRecovery)
		{
			UObjectPoolModuleStatics::DespawnObject(EventHandle);
		}
	}
}

UEventManagerBase* UEventModule::GetEventManager(TSubclassOf<UEventManagerBase> InClass) const
{
	const FName EventManagerName = InClass->GetDefaultObject<UEventManagerBase>()->GetEventManagerName();
	return GetEventManagerByName(EventManagerName, InClass);
}

UEventManagerBase* UEventModule::GetEventManagerByName(const FName InName, TSubclassOf<UEventManagerBase> InClass) const
{
	if(EventManagerRefs.Contains(InName))
	{
		return GetDeterminesOutputObject(EventManagerRefs[InName], InClass);
	}
	return nullptr;
}

void UEventModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}
