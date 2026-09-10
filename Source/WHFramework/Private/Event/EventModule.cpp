#include "Event/EventModule.h"

#include "Event/EventModuleNetworkComponent.h"
#include "Event/EventModuleTypes.h"
#include "Event/Manager/DefaultEventManagerBase.h"
#include "Event/Manager/EventManagerBase.h"

IMPLEMENTATION_MODULE(UEventModule)

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
	for(UEventManagerBase* EventManager : EventManagers)
	{
		EventManager->OnInitialize();
		EventManagerRefs.Add(EventManager->GetEventManagerName(), EventManager);
	}
}

void UEventModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
	if(PHASEC(InPhase, EPhase::Final))
	{
		for(const auto& Pair : EventManagerRefs)
		{
			Pair.Value->OnPreparatory();
		}
	}
}

void UEventModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);
	if(bInEditor) return;
	for(const auto& Pair : EventManagerRefs)
	{
		Pair.Value->OnRefresh(DeltaSeconds);
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
		for(const auto& Pair : EventManagerRefs)
		{
			Pair.Value->OnTermination(InPhase);
		}
		EventMappings.Empty();
	}
}

FDelegateHandle UEventModule::SubscribeEvent(UObject* InOwner, const UScriptStruct* InEventType, FEventDelegate InCallback)
{
	if(!InOwner || !InEventType || !InEventType->IsChildOf(FEventBase::StaticStruct()) || !InCallback.IsBound()) return {};

	FEventListener Listener;
	Listener.Owner = InOwner;
	Listener.Handle = FDelegateHandle(FDelegateHandle::GenerateNewHandle);
	Listener.Delegate = MoveTemp(InCallback);
	FEventMapping& Mapping = EventMappings.FindOrAdd(InEventType);
	Mapping.Listeners.Add(MoveTemp(Listener));
	return Mapping.Listeners.Last().Handle;
}

void UEventModule::UnsubscribeEvent(FDelegateHandle InHandle)
{
	for(auto& Pair : EventMappings)
	{
		Pair.Value.Listeners.RemoveAll([InHandle](const FEventListener& InListener)
		{
			return InListener.Handle == InHandle;
		});
	}
}

void UEventModule::UnsubscribeEvent(const UScriptStruct* InEventType, UObject* InOwner)
{
	if(FEventMapping* Mapping = EventMappings.Find(InEventType))
	{
		Mapping->Listeners.RemoveAll([InOwner](const FEventListener& InListener)
		{
			return InListener.Owner == InOwner;
		});
	}
}

void UEventModule::UnsubscribeAllEvent()
{
	EventMappings.Empty();
}

void UEventModule::BroadcastEvent(UObject* InSender, FConstStructView InEvent, EEventNetType InNetType)
{
	if(!InEvent.IsValid() || !InEvent.GetScriptStruct()->IsChildOf(FEventBase::StaticStruct())) return;
	if(InNetType == EEventNetType::Local)
	{
		BroadcastEventInternal(InSender, InEvent);
		return;
	}

	UEventModuleNetworkComponent* NetworkComponent = GetModuleNetworkComponent<UEventModuleNetworkComponent>();
	if(!NetworkComponent) return;
	FEventNetworkMessage Message;
	Message.Event = FInstancedStruct(InEvent);
	switch(InNetType)
	{
		case EEventNetType::Server:
			NetworkComponent->ServerBroadcastEvent(Message);
			break;
		case EEventNetType::Client:
			NetworkComponent->ClientBroadcastEvent(Message);
			break;
		case EEventNetType::Multicast:
			NetworkComponent->MulticastBroadcastEvent(Message);
			break;
		default:
			break;
	}
}

void UEventModule::BroadcastEventInternal(UObject* InSender, FConstStructView InEvent)
{
	FEventMapping* Mapping = EventMappings.Find(InEvent.GetScriptStruct());
	if(!Mapping) return;
	for(int32 Index = Mapping->Listeners.Num() - 1; Index >= 0; --Index)
	{
		FEventListener& Listener = Mapping->Listeners[Index];
		if(!Listener.Owner.IsValid())
		{
			Mapping->Listeners.RemoveAt(Index);
			continue;
		}
		Listener.Delegate.Execute(InSender, InEvent);
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
