#include "Event/EventModuleNetworkComponent.h"

#include "Event/EventModule.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UObject/GCObject.h"

namespace
{
	class FIncomingEventReferences final : public FGCObject
	{
	public:
		explicit FIncomingEventReferences(FInstancedStruct& InEvent) : Event(InEvent) {}
		virtual void AddReferencedObjects(FReferenceCollector& Collector) override { Event.AddStructReferencedObjects(Collector); }
		virtual FString GetReferencerName() const override { return TEXT("FIncomingEventReferences"); }
	private:
		FInstancedStruct& Event;
	};
}

UEventModuleNetworkComponent::UEventModuleNetworkComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UEventModuleNetworkComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	MaintainLocalWorldRoute();
}

void UEventModuleNetworkComponent::ServerBroadcastEvent_Implementation(FGuid WorldRoute, const FEventNetworkMessage& Message)
{
	HandleClientEvent(WorldRoute, Message, false);
}

void UEventModuleNetworkComponent::ClientBroadcastEvent_Implementation(FGuid WorldRoute, const FEventNetworkMessage& Message)
{
	if (AcceptLocalWorldRoute(WorldRoute) && Message.Event.IsValid() && Message.Event.GetScriptStruct()->IsChildOf(FEventBase::StaticStruct()))
	{
		if (UEventModule* Events = UEventModule::GetPtr(GetWorld()))
			Events->BroadcastEventInternal(nullptr, FConstStructView(Message.Event.GetScriptStruct(), Message.Event.GetMemory()));
	}
}

void UEventModuleNetworkComponent::ServerRequestMulticastEvent_Implementation(FGuid WorldRoute, const FEventNetworkMessage& Message)
{
	HandleClientEvent(WorldRoute, Message, true);
}

void UEventModuleNetworkComponent::HandleClientEvent(FGuid WorldRoute, const FEventNetworkMessage& Message, bool bMulticast)
{
	if (!AcceptRemoteWorldRoute(WorldRoute) || !Message.Event.IsValid() || !Message.Event.GetScriptStruct()->IsChildOf(FEventBase::StaticStruct())) return;
	UWorld* World = GetWorld();
	UEventModule* Events = UEventModule::GetPtr(World);
	APlayerController* Controller = Cast<APlayerController>(GetOwner());
	if (!Events || !Controller) return;
	FEventNetworkMessage StableMessage = Message;
	FIncomingEventReferences References(StableMessage.Event);
	const FConstStructView Event(StableMessage.Event.GetScriptStruct(), StableMessage.Event.GetMemory());
	if (!Events->CanAcceptClientEvent(Controller, Event)) return;
	if (bMulticast) Events->MulticastWorldEvent(StableMessage);
	else Events->BroadcastEventInternal(Controller, Event);
}
