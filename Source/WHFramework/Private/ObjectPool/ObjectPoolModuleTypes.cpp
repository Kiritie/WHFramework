#include "ObjectPool/ObjectPoolModuleTypes.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"

namespace
{
	TMap<TWeakObjectPtr<UClass>, TWeakObjectPtr<UScriptStruct>> GObjectPoolSpawnParameterTypes;
	TMap<TWeakObjectPtr<UClass>, FObjectPoolPolicy> GObjectPoolPolicies;
}

void FObjectPoolPolicyRegistry::Register(UClass* InClass, const FObjectPoolPolicy& InPolicy)
{
	if(InClass)
	{
		GObjectPoolPolicies.Add(InClass, InPolicy);
	}
}

void FObjectPoolPolicyRegistry::Unregister(UClass* InClass)
{
	GObjectPoolPolicies.Remove(InClass);
}

FObjectPoolPolicy FObjectPoolPolicyRegistry::Resolve(UClass* InClass)
{
	for(UClass* Class = InClass; Class; Class = Class->GetSuperClass())
	{
		if(const FObjectPoolPolicy* Policy = GObjectPoolPolicies.Find(Class))
		{
			return *Policy;
		}
	}
	return FObjectPoolPolicy();
}

void FObjectPoolSpawnParameterRegistry::Register(UClass* InClass, UScriptStruct* InStruct)
{
	if(!InClass || !InStruct || InStruct == FSpawnParameter::StaticStruct() ||
		!InStruct->IsChildOf(FSpawnParameter::StaticStruct()))
	{
		return;
	}

	GObjectPoolSpawnParameterTypes.Add(InClass, InStruct);
}

void FObjectPoolSpawnParameterRegistry::Unregister(UClass* InClass)
{
	GObjectPoolSpawnParameterTypes.Remove(InClass);
}

void FObjectPoolSpawnParameterRegistry::Clear()
{
	GObjectPoolSpawnParameterTypes.Empty();
}

UScriptStruct* FObjectPoolSpawnParameterRegistry::Resolve(UClass* InClass)
{
	for(UClass* Class = InClass; Class; Class = Class->GetSuperClass())
	{
		if(const TWeakObjectPtr<UScriptStruct>* Struct = GObjectPoolSpawnParameterTypes.Find(Class))
		{
			if(Struct->IsValid())
			{
				return Struct->Get();
			}
		}
	}

	if(InClass && InClass->IsChildOf<UUserWidget>())
	{
		return FWidgetSpawnParameter::StaticStruct();
	}

	if(InClass && InClass->IsChildOf<AActor>())
	{
		return FActorSpawnParameter::StaticStruct();
	}

	return InClass ? FObjectSpawnParameter::StaticStruct() : nullptr;
}
