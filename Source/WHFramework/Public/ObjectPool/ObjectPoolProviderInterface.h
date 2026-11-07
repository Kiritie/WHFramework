#pragma once

#include "CoreMinimal.h"
#include "Parameter/ParameterModuleTypes.h"

class UObjectPoolModule;

class IObjectPoolProvider
{
public:
	virtual ~IObjectPoolProvider() = default;

	virtual UObject* Spawn(UClass* InClass, const FParameter& InParam) = 0;

	virtual void PrepareDespawn(UObject* InObject) { }

	virtual bool Despawn(UObject* InObject) = 0;

	virtual void Destroy(UObject* InObject) = 0;

	virtual void ClearAll() = 0;
};
