#include "Event/EventModule.h"
#include "Common/CommonTypes.h"
#include "WHFrameworkCoreMacros.h"
#include "Common/LifecycleTypes.h"

#include "Event/EventModuleNetworkComponent.h"
#include "Event/EventModuleTypes.h"
#include "Event/Manager/DefaultEventManagerBase.h"
#include "Event/Manager/EventManagerBase.h"
#include "Main/MainModule.h"
#include "Main/WorldNetworkRecipients.h"
#include "Main/WorldNetworkRoute.h"
#include "Engine/NetConnection.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "UObject/GCObject.h"

IMPLEMENTATION_MODULE(UEventModule)

namespace
{
	class FEventPayloadReferences final : public FGCObject
	{
	public:
		explicit FEventPayloadReferences(FInstancedStruct& InPayload) : Payload(InPayload) {}

		virtual void AddReferencedObjects(FReferenceCollector& Collector) override
		{
			Payload.AddStructReferencedObjects(Collector);
		}

		virtual FString GetReferencerName() const override
		{
			return TEXT("FEventPayloadReferences");
		}

	private:
		FInstancedStruct& Payload;
	};
}

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
}

#if WITH_EDITOR
void UEventModule::OnGenerate()
{
	Super::OnGenerate();
}

void UEventModule::OnDestroy()
{
	Super::OnDestroy();
}
#endif

void UEventModule::OnInitialize()
{
	bEventDeliveryClosing = false;
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
	bEventDeliveryClosing = true;
	EventMappings.Empty();
	Super::OnTermination(InPhase);
	if(PHASEC(InPhase, EPhase::Final))
	{
		for(const auto& Pair : EventManagerRefs)
		{
			Pair.Value->OnTermination(InPhase);
		}
	}
}

FDelegateHandle UEventModule::SubscribeEvent(UObject* InOwner, const UScriptStruct* InEventType, FEventDelegate InCallback)
{
	if (IsEventDeliveryClosing() || !::IsValid(InOwner) || !InEventType ||
		!InEventType->IsChildOf(FEventBase::StaticStruct()) || !InCallback.IsBound()) return {};

	FEventListener Listener;
	Listener.Owner = InOwner;
	Listener.Handle = FDelegateHandle(FDelegateHandle::GenerateNewHandle);
	Listener.Delegate = MoveTemp(InCallback);
	FEventMapping& Mapping = EventMappings.FindOrAdd(InEventType);
	if (!Mapping.EventType.IsValid()) Mapping.EventType.Reset(const_cast<UScriptStruct*>(InEventType));
	Mapping.Listeners.Add(MoveTemp(Listener));
	return Mapping.Listeners.Last().Handle;
}

void UEventModule::UnsubscribeEvent(FDelegateHandle InHandle)
{
	for (auto It = EventMappings.CreateIterator(); It; ++It)
	{
		It.Value().Listeners.RemoveAll([InHandle](const FEventListener& InListener)
		{
			return InListener.Handle == InHandle;
		});
		if (It.Value().Listeners.IsEmpty()) It.RemoveCurrent();
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
		if (Mapping->Listeners.IsEmpty()) EventMappings.Remove(InEventType);
	}
}

void UEventModule::UnsubscribeAllEvent()
{
	EventMappings.Empty();
}

FNetworkDispatchResult UEventModule::BroadcastEvent(UObject* InSender, FConstStructView InEvent, EEventNetType InNetType, APlayerController* InController)
{
	UWorld* World = GetWorld();
	if (IsEventDeliveryClosing() || !World)
		return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::InvalidContext);
	if (!InEvent.IsValid() || !InEvent.GetScriptStruct()->IsChildOf(FEventBase::StaticStruct()))
		return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::InvalidRequest);

	AMainModule* Owner = GetModuleOwner();
	if (!Owner) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::ModuleUnavailable);
	FNetworkDispatchResult Result;
	Result.WorldEpoch = WorldNetworkRoute::GetEpoch(World);
	const bool bAuthority = Owner->HasAuthority();
	if (InNetType == EEventNetType::Local || (InNetType == EEventNetType::Server && bAuthority) ||
		(InNetType == EEventNetType::Client && bAuthority && InController && InController->IsLocalController()))
	{
		Result.bLocalDispatched = true;
		BroadcastEventInternal(InSender, InEvent);
		Result.Status = ENetworkDispatchStatus::LocalDispatched;
		return Result;
	}
	FEventNetworkMessage Message;
	Message.Event = FInstancedStruct(InEvent);
	FEventPayloadReferences MessageReferences(Message.Event);
	if (InNetType == EEventNetType::Multicast && bAuthority) return MulticastWorldEvent(Message);
	if ((InNetType == EEventNetType::Client && !bAuthority) ||
		((InNetType == EEventNetType::Server || InNetType == EEventNetType::Multicast) && bAuthority) ||
		!InController || (!bAuthority && !InController->IsLocalController()))
		return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::NotAuthorized);

	UEventModuleNetworkComponent* Network = GetModuleNetworkComponent<UEventModuleNetworkComponent>(InController);
	if (!Network) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::ModuleUnavailable);
	const FGuid Route = bAuthority ? Network->GetRemoteWorldRoute() : Network->MaintainLocalWorldRoute();
	if (!Route.IsValid()) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::RouteUnavailable);
	const FGuid CurrentEpoch = WorldNetworkRoute::GetEpoch(World);
	if (!CurrentEpoch.IsValid() || (Result.WorldEpoch.IsValid() && CurrentEpoch != Result.WorldEpoch)) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::Interrupted);
	Result.WorldEpoch = CurrentEpoch;
	switch (InNetType)
	{
		case EEventNetType::Server:
			Network->ServerBroadcastEvent(Route, Message);
			break;
		case EEventNetType::Client:
			Network->ClientBroadcastEvent(Route, Message);
			break;
		case EEventNetType::Multicast:
			Network->ServerRequestMulticastEvent(Route, Message);
			break;
		default:
			return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::InvalidRequest);
	}
	Result.Status = ENetworkDispatchStatus::RpcDispatched;
	Result.RemoteDispatched = 1;
	return Result;
}

FNetworkDispatchResult UEventModule::MulticastWorldEvent(const FEventNetworkMessage& InMessage)
{
	if (IsEventDeliveryClosing() || !GetModuleOwner() || !GetModuleOwner()->HasAuthority())
		return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::NotAuthorized);
	if (!InMessage.Event.IsValid() || !InMessage.Event.GetScriptStruct()->IsChildOf(FEventBase::StaticStruct()))
		return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::InvalidRequest);
	UWorld* World = GetWorld();
	FNetworkDispatchResult Result;
	Result.WorldEpoch = WorldNetworkRoute::GetEpoch(World);
	struct FRecipient
	{
		TWeakObjectPtr<APlayerController> Player;
		TWeakObjectPtr<UNetConnection> Connection;
	};
	TArray<FRecipient> Recipients;
	for (APlayerController* Player : WorldNetworkRecipients::Gather(World))
		Recipients.Add({Player, Player ? Player->GetNetConnection() : nullptr});
	FEventNetworkMessage Message = InMessage;
	FEventPayloadReferences MessageReferences(Message.Event);
	Result.bLocalDispatched = true;
	BroadcastEventInternal(nullptr, FConstStructView(Message.Event.GetScriptStruct(), Message.Event.GetMemory()));
	for (int32 Index = 0; Index < Recipients.Num(); ++Index)
	{
		APlayerController* Player = Recipients[Index].Player.Get();
		UNetConnection* Connection = Recipients[Index].Connection.Get();
		UEventModuleNetworkComponent* Network = Player && Connection &&
			Player->GetNetConnection() == Connection ? GetModuleNetworkComponent<UEventModuleNetworkComponent>(Player) : nullptr;
		const FGuid Route = Network ? Network->GetRemoteWorldRoute() : FGuid();
		if (!Result.WorldEpoch.IsValid() || !Route.IsValid())
		{
			++Result.RemoteUnavailable;
			continue;
		}
		Network->ClientBroadcastEvent(Route, Message);
		++Result.RemoteDispatched;
	}
	Result.Status = Result.RemoteUnavailable ? ENetworkDispatchStatus::PartiallyDispatched :
		Result.RemoteDispatched ? ENetworkDispatchStatus::RpcDispatched : ENetworkDispatchStatus::LocalDispatched;
	return Result;
}

void UEventModule::BroadcastEventInternal(UObject* InSender, FConstStructView InEvent)
{
	if (IsEventDeliveryClosing() || !InEvent.IsValid() || !InEvent.GetScriptStruct()->IsChildOf(FEventBase::StaticStruct())) return;
	const TWeakObjectPtr<UObject> Sender = InSender;
	const UScriptStruct* EventType = InEvent.GetScriptStruct();
	FInstancedStruct Payload(InEvent);
	FEventPayloadReferences PayloadReferences(Payload);
	const FEventMapping* Mapping = EventMappings.Find(EventType);
	if (!Mapping) return;
	TArray<FDelegateHandle> Pending;
	Pending.Reserve(Mapping->Listeners.Num());
	for (int32 Index = Mapping->Listeners.Num() - 1; Index >= 0; --Index) Pending.Add(Mapping->Listeners[Index].Handle);
	for (const FDelegateHandle Handle : Pending)
	{
		if (IsEventDeliveryClosing()) return;
		FEventMapping* Current = EventMappings.Find(EventType);
		FEventListener* Listener = Current ? Current->Listeners.FindByPredicate([Handle](const FEventListener& Entry) { return Entry.Handle == Handle; }) : nullptr;
		if (!Listener) continue;
		if (!Listener->Owner.IsValid())
		{
			Current->Listeners.RemoveAll([Handle](const FEventListener& Entry) { return Entry.Handle == Handle; });
			if (Current->Listeners.IsEmpty()) EventMappings.Remove(EventType);
			continue;
		}
		const FEventDelegate Callback = Listener->Delegate;
		Callback.ExecuteIfBound(Sender.Get(), FConstStructView(EventType, Payload.GetMemory()));
	}
}

UEventManagerBase* UEventModule::GetEventManager(TSubclassOf<UEventManagerBase> InClass) const
{
	if (!InClass) return nullptr;
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
