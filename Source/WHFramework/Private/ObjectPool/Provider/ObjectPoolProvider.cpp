#include "ObjectPool/Provider/ObjectPoolProvider.h"

#include "ObjectPool/ObjectPoolBucket.h"
#include "ObjectPool/ObjectPoolModule.h"
#include "ObjectPool/ObjectPoolModuleTypes.h"

FObjectPoolProvider::FObjectPoolProvider(UObjectPoolModule& InModule)
	: Module(InModule)
{
}

UObject* FObjectPoolProvider::Spawn(UClass* InClass, const FParameter& InParam)
{
	const FObjectSpawnParameter* Parameter = InParam.GetPtr<FObjectSpawnParameter>();
	UObject* Outer = Parameter && Parameter->Outer ? Parameter->Outer.Get() : &Module;
	const FObjectPoolPolicy Policy = Module.GetPoolPolicy(InClass);
	UObject* Object = nullptr;

	if(Policy.bEnablePooling)
	{
		Object = Module.FindOrAddGenericBucket(InClass, Outer, Policy.MaxIdle)->Acquire();
	}

	return Object ? Object : NewObject<UObject>(Outer, InClass);
}

bool FObjectPoolProvider::Despawn(UObject* InObject)
{
	const FObjectPoolPolicy Policy = Module.GetPoolPolicy(InObject->GetClass());
	if(!Policy.bEnablePooling)
	{
		return false;
	}

	return Module.FindOrAddGenericBucket(
		InObject->GetClass(),
		InObject->GetOuter(),
		Policy.MaxIdle)->Release(InObject);
}

void FObjectPoolProvider::Destroy(UObject* InObject)
{
	Module.RemoveFromGenericBucket(InObject);
}

void FObjectPoolProvider::ClearAll()
{
}
