// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/EventModule.h"

#include "Main/MainModule.h"
#include "Debug/DebugModuleTypes.h"
#include "Event/EventModuleStatics.h"
#include "Event/EventModuleNetworkComponent.h"
#include "Event/Manager/EventManagerBase.h"
#include "Event/Handle/EventHandleBase.h"
#include "Common/CommonModuleStatics.h"
#include "Event/Manager/DefaultEventManagerBase.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"

IMPLEMENTATION_MODULE(UEventModule)

// ParamSets default values
UEventModule::UEventModule()
{
	ModuleName = FName("EventModule");
	ModuleDisplayName = FText::FromString(TEXT("Event Module"));

	bModuleRequired = true;

	ModuleNetworkComponent = UEventModuleNetworkComponent::StaticClass();

	EventManagers = TArray<UEventManagerBase*>();
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
	
	if(EventManagers.IsEmpty())
	{
		EventManagers.Add(NewObject<UDefaultEventManagerBase>(this));
	}

	for(auto Iter : EventManagers)
	{
		Iter->OnInitialize();
		EventManagerRefs.Add(Iter->GetEventManagerName(), Iter);
	}
}

void UEventModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);

	if(PHASEC(InPhase, EPhase::Final))
	{
		for(auto Iter : EventManagerRefs)
		{
			Iter.Value->OnPreparatory();
		}
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
	
	if(PHASEC(InPhase, EPhase::Final))
	{
		for(auto Iter : EventManagerRefs)
		{
			Iter.Value->OnTermination(InPhase);
		}
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
	
	if(UEventHandleBase* EventHandle = UObjectPoolModuleStatics::SpawnObject<UEventHandleBase>(nullptr, nullptr, InClass))
	{
		EventHandle->Parse(InParams);
		
		struct
		{
			UObject* Sender;
			UEventHandleBase* EventHandle;
		} Params { InSender, EventHandle };

		TArray<TPair<TWeakObjectPtr<UObject>, FName>> FuncEntries;
		if(const FEventMapping* Mapping = EventMappings.Find(InClass))
		{
			for (const auto& Iter1 : Mapping->FuncMap)
			{
				for (const auto& Iter2 : Iter1.Value.FuncNames)
				{
					FuncEntries.Emplace(Iter1.Key, Iter2);
				}
			}
		}

		for (const auto& Iter : FuncEntries)
		{
			UObject* Owner = Iter.Key.Get();
			const FEventMapping* CurrentMapping = EventMappings.Find(InClass);
			const FEventFuncs* CurrentFuncs = Owner && CurrentMapping ? CurrentMapping->FuncMap.Find(Owner) : nullptr;
			if(!CurrentFuncs || !CurrentFuncs->FuncNames.Contains(Iter.Value)) continue;

			const FString OwnerClassName = Owner->GetClass()->GetName();
			if (EventHandle->Filter(Owner, Iter.Value) && UCommonModuleStatics::ExecuteObjectFunc(Owner, Iter.Value, &Params))
			{
				WHLog(FString::Printf(TEXT("ExecuteEvent : FuncName : %s, EventOwner : %s"), *Iter.Value.ToString(), *OwnerClassName), EDC_Event);
			}
		}

		if(bRecovery && !EventHandle->OnExecute())
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
