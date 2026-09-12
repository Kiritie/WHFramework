#pragma once

#include "ObjectPool/ObjectPoolProvider.h"

class UObjectPoolWidgetBucket;

class FWidgetPoolProvider : public IObjectPoolProvider
{
public:
	explicit FWidgetPoolProvider(UObjectPoolModule& InModule);

	virtual UObject* Spawn(UClass* InClass, const FParameter& InParam) override;

	virtual bool Despawn(UObject* InObject) override;

	virtual void Destroy(UObject* InObject) override;

	virtual void ClearAll() override;

private:
	UObjectPoolModule& Module;

	TMap<TWeakObjectPtr<UUserWidget>, TWeakObjectPtr<UObjectPoolWidgetBucket>> ActiveBuckets;
};
