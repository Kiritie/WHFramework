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
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(Server, Reliable)
	void ServerBroadcastEvent(FGuid WorldRoute, const FEventNetworkMessage& Message);

	UFUNCTION(Client, Reliable)
	void ClientBroadcastEvent(FGuid WorldRoute, const FEventNetworkMessage& Message);

	UFUNCTION(Server, Reliable)
	void ServerRequestMulticastEvent(FGuid WorldRoute, const FEventNetworkMessage& Message);

private:
	void HandleClientEvent(FGuid WorldRoute, const FEventNetworkMessage& Message, bool bMulticast);
};
