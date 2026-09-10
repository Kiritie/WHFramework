#include "Event/EventModuleNetworkComponent.h"

#include "Event/EventModule.h"

UEventModuleNetworkComponent::UEventModuleNetworkComponent()
{
}

void UEventModuleNetworkComponent::ServerBroadcastEvent_Implementation(const FEventNetworkMessage& Message)
{
	if(Message.Event.IsValid())
	{
		UEventModule::Get().BroadcastEventInternal(nullptr, FConstStructView(Message.Event.GetScriptStruct(), Message.Event.GetMemory()));
	}
}

void UEventModuleNetworkComponent::ClientBroadcastEvent_Implementation(const FEventNetworkMessage& Message)
{
	if(Message.Event.IsValid())
	{
		UEventModule::Get().BroadcastEventInternal(nullptr, FConstStructView(Message.Event.GetScriptStruct(), Message.Event.GetMemory()));
	}
}

void UEventModuleNetworkComponent::MulticastBroadcastEvent_Implementation(const FEventNetworkMessage& Message)
{
	if(Message.Event.IsValid())
	{
		UEventModule::Get().BroadcastEventInternal(nullptr, FConstStructView(Message.Event.GetScriptStruct(), Message.Event.GetMemory()));
	}
}
