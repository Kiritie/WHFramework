#include "ObjectPool/ObjectPoolModuleStatics.h"

UObject* UObjectPoolModuleStatics::SpawnObject(UClass* InClass, const FParameter& InParam)
{
	return UObjectPoolModule::Get().SpawnObject(InClass, InParam);
}

void UObjectPoolModuleStatics::DespawnObject(UObject* InObject, EObjectDespawnMode InMode)
{
	UObjectPoolModule::Get().DespawnObject(InObject, InMode);
}

void UObjectPoolModuleStatics::ClearObject(TSubclassOf<UObject> InClass)
{
	UObjectPoolModule::Get().ClearObject(InClass);
}

void UObjectPoolModuleStatics::ClearAllObject()
{
	UObjectPoolModule::Get().ClearAllObject();
}
