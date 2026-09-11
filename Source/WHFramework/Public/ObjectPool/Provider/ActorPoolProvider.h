#pragma once

#include "ObjectPool/ObjectPoolProvider.h"

struct FActorPoolState
{
	bool bHidden = false;
	bool bCollisionEnabled = true;
	bool bTickEnabled = true;
};

class FActorPoolProvider : public IObjectPoolProvider
{
public:
	explicit FActorPoolProvider(UObjectPoolModule& InModule);

	virtual UObject* Spawn(UClass* InClass, const FParameter& InParameter) override;

	virtual void PrepareDespawn(UObject* InObject) override;

	virtual bool Despawn(UObject* InObject) override;

	virtual void Destroy(UObject* InObject) override;

	virtual void ClearAll() override;

private:
	UObjectPoolModule& Module;

	TMap<TWeakObjectPtr<AActor>, FActorPoolState> ActorStates;
};
