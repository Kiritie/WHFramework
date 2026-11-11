#include "ObjectPool/ObjectPoolModuleStatics.h"

UObject* UObjectPoolModuleStatics::SpawnObject(const UObject* InWorldContext, UClass* InClass, const FParameter& InParam)
{
	UObjectPoolModule* Pool = InWorldContext ? UObjectPoolModule::GetPtr(InWorldContext->GetWorld()) : nullptr;
	return Pool ? Pool->SpawnObject(InClass, InParam) : nullptr;
}

void UObjectPoolModuleStatics::DespawnObject(const UObject* InWorldContext, UObject* InObject, EObjectDespawnMode InMode)
{
	if (UObjectPoolModule* Pool = InWorldContext ? UObjectPoolModule::GetPtr(InWorldContext->GetWorld()) : nullptr) Pool->DespawnObject(InObject, InMode);
}

void UObjectPoolModuleStatics::ClearObject(const UObject* InWorldContext, TSubclassOf<UObject> InClass)
{
	if (UObjectPoolModule* Pool = InWorldContext ? UObjectPoolModule::GetPtr(InWorldContext->GetWorld()) : nullptr) Pool->ClearObject(InClass);
}

void UObjectPoolModuleStatics::ClearAllObject(const UObject* InWorldContext)
{
	if (UObjectPoolModule* Pool = InWorldContext ? UObjectPoolModule::GetPtr(InWorldContext->GetWorld()) : nullptr) Pool->ClearAllObject();
}
