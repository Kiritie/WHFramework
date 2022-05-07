// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/EventModule.h"

#include "Main/MainModule.h"
#include "Debug/DebugModuleTypes.h"
#include "Engine/ActorChannel.h"
#include "Event/EventModuleBPLibrary.h"
#include "Event/EventModuleNetworkComponent.h"
#include "Event/Manager/EventManagerBase.h"
#include "Event/Handle/EventHandleBase.h"
#include "ObjectPool/ObjectPoolModuleBPLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Network/NetworkModuleBPLibrary.h"

// ParamSets default values
AEventModule::AEventModule()
{
	ModuleName = FName("EventModule");
	
	EventManagerClass = AEventManagerBase::StaticClass();
}

#if WITH_EDITOR
void AEventModule::OnGenerate_Implementation()
{
	Super::OnGenerate_Implementation();
}

void AEventModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
}
#endif

void AEventModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();

	if(HasAuthority())
	{
		SpawnEventManager();
	}
}

void AEventModule::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();
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

void AEventModule::SubscribeEvent(TSubclassOf<UEventHandleBase> InEventHandleClass, UObject* InOwner, const FName InFuncName)
{
	if(!InEventHandleClass || !InOwner || InFuncName.IsNone()) return;

	if(!EventHandleInfos.Contains(InEventHandleClass))
	{
		EventHandleInfos.Add(InEventHandleClass, FEventHandleInfo(FEventHandleDelegate()));
	}

	if(!EventHandleInfos[InEventHandleClass].EventHandleDelegate.IsBoundToObject(this))
	{
		EventHandleInfos[InEventHandleClass].EventHandleDelegate.BindUFunction(this, FName("ExecuteEvent"));
	}

	switch (InEventHandleClass->GetDefaultObject<UEventHandleBase>()->EventType)
	{
		case EEventType::Single:
		{
			EventHandleInfos[InEventHandleClass].EventHandleFuncMap.Empty();
		}
		case EEventType::Multicast:
		{
			if(!EventHandleInfos[InEventHandleClass].EventHandleFuncMap.Contains(InOwner))
			{
				EventHandleInfos[InEventHandleClass].EventHandleFuncMap.Add(InOwner, FEventHandleFuncs());
			}
			EventHandleInfos[InEventHandleClass].EventHandleFuncMap[InOwner].FuncNames.Add(InFuncName);
			break;
		}
	}
}

void AEventModule::UnsubscribeEvent(TSubclassOf<UEventHandleBase> InEventHandleClass, UObject* InOwner, const FName InFuncName)
{
	if(!InEventHandleClass || !InOwner || InFuncName.IsNone()) return;

	if(!EventHandleInfos.Contains(InEventHandleClass))
	{
		EventHandleInfos.Add(InEventHandleClass, FEventHandleInfo(FEventHandleDelegate()));
	}
	
	if(EventHandleInfos[InEventHandleClass].EventHandleFuncMap.Contains(InOwner))
	{
		if(EventHandleInfos[InEventHandleClass].EventHandleFuncMap[InOwner].FuncNames.Contains(InFuncName))
		{
			EventHandleInfos[InEventHandleClass].EventHandleFuncMap[InOwner].FuncNames.Remove(InFuncName);
		}
		if(EventHandleInfos[InEventHandleClass].EventHandleFuncMap[InOwner].FuncNames.Num() == 0)
		{
			EventHandleInfos[InEventHandleClass].EventHandleFuncMap.Remove(InOwner);
		}
	}

	if(EventHandleInfos[InEventHandleClass].EventHandleFuncMap.Num() == 0)
	{
		EventHandleInfos[InEventHandleClass].EventHandleDelegate.Unbind();
	}
}

void AEventModule::UnsubscribeAllEvent()
{
	EventHandleInfos.Empty();
}

void AEventModule::BroadcastEvent(TSubclassOf<UEventHandleBase> InEventHandleClass, EEventNetType InEventNetType, UObject* InSender, const TArray<FParameter>* InParams)
{
	BroadcastEvent(InEventHandleClass, InEventNetType, InSender, InParams ? *InParams : TArray<FParameter>());
}

void AEventModule::BroadcastEvent(TSubclassOf<UEventHandleBase> InEventHandleClass, EEventNetType InEventNetType, UObject* InSender, const TArray<FParameter>& InParams)
{
	if(!InEventHandleClass) return;

	if(!EventHandleInfos.Contains(InEventHandleClass))
	{
		EventHandleInfos.Add(InEventHandleClass, FEventHandleInfo(FEventHandleDelegate()));
	}

	switch(InEventNetType)
	{
		case EEventNetType::Client:
		{
			if(UEventModuleNetworkComponent* EventModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UEventModuleNetworkComponent>())
			{
				EventModuleNetworkComponent->ClientBroadcastEvent(InSender, InEventHandleClass, InParams);
				return;
			}
		}
		case EEventNetType::Server:
		{
			if(UEventModuleNetworkComponent* EventModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UEventModuleNetworkComponent>())
			{
				EventModuleNetworkComponent->ServerBroadcastEvent(InSender, InEventHandleClass, InParams);
				return;
			}
		}
		case EEventNetType::Multicast:
		{
			if(UEventModuleNetworkComponent* EventModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UEventModuleNetworkComponent>())
			{
				EventModuleNetworkComponent->ServerBroadcastEventMulticast(InSender, InEventHandleClass, InParams);
				return;
			}
		}
		default: break;
	}
	
	EventHandleInfos[InEventHandleClass].EventHandleDelegate.ExecuteIfBound(InEventHandleClass, InSender, InParams);
}

void AEventModule::MultiBroadcastEvent_Implementation(TSubclassOf<UEventHandleBase> InEventHandleClass, UObject* InSender, const TArray<FParameter>& InParams)
{
	BroadcastEvent(InEventHandleClass, EEventNetType::Single, InSender, InParams);
}

void AEventModule::ExecuteEvent(TSubclassOf<UEventHandleBase> InEventHandleClass, UObject* InSender, const TArray<FParameter>& InParams)
{
	if(!EventHandleInfos.Contains(InEventHandleClass)) return;
	
	if(UEventHandleBase* EventHandle = UObjectPoolModuleBPLibrary::SpawnObject<UEventHandleBase>(nullptr, InEventHandleClass))
	{
		EventHandle->Fill(InParams);
		
		struct
		{
			UObject* Sender;
			UEventHandleBase* EventHandle;
		} Params{InSender, EventHandle};

		for (auto Iter1 : EventHandleInfos[InEventHandleClass].EventHandleFuncMap)
		{
			for (auto Iter2 : Iter1.Value.FuncNames)
			{
				if (EventHandle->Filter(Iter1.Key, Iter2) && UGlobalBPLibrary::ExecuteObjectFunc(Iter1.Key, Iter2, &Params))
				{
					WH_LOG(WH_Event, Log, TEXT("ExecuteEvent : FuncName : %s, EventOwner : %s"), *Iter2.ToString(), *Iter1.Key->GetClass()->GetName());
				}
			}
		}

		UObjectPoolModuleBPLibrary::DespawnObject(EventHandle);
	}
}

void AEventModule::SpawnEventManager()
{
	if(EventManagerClass && (!EventManager || !EventManager->IsA(EventManagerClass)))
	{
		DestroyEventManager();
		
		FActorSpawnParameters ActorSpawnParameters = FActorSpawnParameters();
        ActorSpawnParameters.Owner = this;
        ActorSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		EventManager = GetWorld()->SpawnActor<AEventManagerBase>(EventManagerClass, ActorSpawnParameters);
        if(EventManager)
        {
        	EventManager->OnInitialize();
#if(WITH_EDITOR)
        	EventManager->SetActorLabel(TEXT("EventManager"));
#endif
        	EventManager->AttachToActor(this, FAttachmentTransformRules::SnapToTargetIncludingScale);
        }
	}
}

void AEventModule::DestroyEventManager()
{
	if(EventManager)
	{
		EventManager->Destroy();
		EventManager = nullptr;
	}
}

void AEventModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AEventModule, EventManager);
}
