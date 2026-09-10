#pragma once

#include "Event/EventModuleTypes.h"
#include "Main/Base/ModuleNetworkComponentBase.h"

#include "EventModuleNetworkComponent.generated.h"

UCLASS()
class WHFRAMEWORK_API UEventModuleNetworkComponent : public UModuleNetworkComponentBase
{
	GENERATED_BODY()

public:
	UEventModuleNetworkComponent();

	UFUNCTION(Server, Reliable)
	void ServerBroadcastEvent(const FEventNetworkMessage& Message);

	UFUNCTION(Client, Reliable)
	void ClientBroadcastEvent(const FEventNetworkMessage& Message);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastBroadcastEvent(const FEventNetworkMessage& Message);
};
