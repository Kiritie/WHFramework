// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"
#include "ModuleNetworkComponentBase.generated.h"

/**
 * 
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class WHFRAMEWORK_API UModuleNetworkComponentBase : public UActorComponent
{
	GENERATED_BODY()

public:
	UModuleNetworkComponentBase();

	// 客户端为其当前 World 建立独立 nonce；旧世界的 RPC 无法在新 World 通过校验。
	FGuid MaintainLocalWorldRoute();
	FGuid GetRemoteWorldRoute() const;
	bool AcceptLocalWorldRoute(const FGuid& InRoute);
	bool AcceptRemoteWorldRoute(const FGuid& InRoute) const;

protected:
	virtual void OnWorldRouteChanged() {}

	UFUNCTION(Server, Reliable)
	void ServerBindWorldRoute(FGuid InWorldEpoch, FGuid InRoute);

private:
	TWeakObjectPtr<UWorld> LocalRouteWorld;
	TWeakObjectPtr<UWorld> RemoteRouteWorld;
	FGuid LocalWorldRoute;
	FGuid RemoteWorldRoute;
	FGuid LocalWorldEpoch;
	FGuid RemoteWorldEpoch;
	double LastWorldRouteBind = -100.0;
};
