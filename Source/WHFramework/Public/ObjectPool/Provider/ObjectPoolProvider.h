#pragma once

#include "ObjectPool/ObjectPoolProvider.h"

class FObjectPoolProvider : public IObjectPoolProvider
{
public:
	explicit FObjectPoolProvider(UObjectPoolModule& InModule);

	virtual UObject* Spawn(UClass* InClass, const FParameter& InParameter) override;

	virtual bool Despawn(UObject* InObject) override;

	virtual void Destroy(UObject* InObject) override;

	virtual void ClearAll() override;

private:
	UObjectPoolModule& Module;
};
