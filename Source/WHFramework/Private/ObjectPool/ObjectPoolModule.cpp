// Fill out your copyright notice in the Description page of Project Settings.


#include "ObjectPool/ObjectPoolModule.h"

#include "ObjectPool/ObjectPool.h"
		
IMPLEMENTATION_MODULE(AObjectPoolModule)

// Sets default values
AObjectPoolModule::AObjectPoolModule()
{
	ModuleName = FName("ObjectPoolModule");

	DefaultLimit = 100;
	ObjectPools = TMap<TSubclassOf<UObject>, UObjectPool*>();
}

AObjectPoolModule::~AObjectPoolModule()
{
	TERMINATION_MODULE(AObjectPoolModule)
}

#if WITH_EDITOR
void AObjectPoolModule::OnGenerate()
{
	Super::OnGenerate();
}

void AObjectPoolModule::OnDestroy()
{
	Super::OnDestroy();
}
#endif

void AObjectPoolModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void AObjectPoolModule::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);
}

void AObjectPoolModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void AObjectPoolModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void AObjectPoolModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

void AObjectPoolModule::OnTermination_Implementation()
{
	Super::OnTermination_Implementation();

	ClearAllObject();
}

bool AObjectPoolModule::HasPool(TSubclassOf<UObject> InType) const
{
	return ObjectPools.Contains(InType);
}

UObjectPool* AObjectPoolModule::GetPool(TSubclassOf<UObject> InType) const
{
	if(HasPool(InType))
	{
		return ObjectPools[InType];
	}
	return nullptr;
}

UObjectPool* AObjectPoolModule::CreatePool(TSubclassOf<UObject> InType)
{
	if(!InType || HasPool(InType)) return nullptr;
	
	UObjectPool* ObjectPool;
	if(InType->IsChildOf<AActor>())
	{
		ObjectPool = NewObject<UActorPool>(this);
	}
	else if(InType->IsChildOf<UUserWidget>())
	{
		ObjectPool = NewObject<UWidgetPool>(this);
	}
	else
	{
		ObjectPool = NewObject<UObjectPool>(this);
	}
	const int32 _Limit = IObjectPoolInterface::Execute_GetLimit(InType.GetDefaultObject());
	ObjectPool->Initialize(_Limit != -1 ? _Limit : DefaultLimit, InType);
	ObjectPools.Add(InType, ObjectPool);
	return ObjectPool;
}

void AObjectPoolModule::DestroyPool(TSubclassOf<UObject> InType)
{
	if(!InType || !HasPool(InType)) return;

	ObjectPools[InType]->ConditionalBeginDestroy();
	ObjectPools.Remove(InType);
}

bool AObjectPoolModule::HasObject(TSubclassOf<UObject> InType)
{
	if(!InType || !InType->ImplementsInterface(UObjectPoolInterface::StaticClass())) return false;

	if(ObjectPools.Contains(InType))
	{
		return ObjectPools[InType]->GetCount() > 0;
	}
	return false;
}

UObject* AObjectPoolModule::SpawnObject(TSubclassOf<UObject> InType, const TArray<FParameter>& InParams)
{
	if(!InType || !InType->ImplementsInterface(UObjectPoolInterface::StaticClass())) return nullptr;

	UObjectPool* ObjectPool;
	if(!HasPool(InType))
	{
		ObjectPool = CreatePool(InType);
	}
	else
	{
		ObjectPool = GetPool(InType);
	}
	return ObjectPool->Spawn(InParams);
}

void AObjectPoolModule::DespawnObject(UObject* InObject, bool bRecovery)
{
	if(!InObject) return;

	UClass* InType = InObject->GetClass();
	if(!InType->ImplementsInterface(UObjectPoolInterface::StaticClass())) return;

	UObjectPool* ObjectPool;
	if(!HasPool(InType))
	{
		ObjectPool = CreatePool(InType);
	}
	else
	{
		ObjectPool = GetPool(InType);
	}
	if(ModuleState != EModuleState::Terminated)
	{
		ObjectPool->Despawn(InObject, bRecovery);
	}
	else
	{
		ObjectPool->DespawnImpl(InObject, false);
	}
}

void AObjectPoolModule::DespawnObjects(TArray<UObject*> InObjects, bool bRecovery)
{
	for(auto Iter : InObjects)
	{
		DespawnObject(Iter, bRecovery);
	}
}

void AObjectPoolModule::ClearObject(TSubclassOf<UObject> InType)
{
	if(!InType || !InType->ImplementsInterface(UObjectPoolInterface::StaticClass())) return;

	if(HasPool(InType))
	{
		GetPool(InType)->Clear();
	}
}

void AObjectPoolModule::ClearAllObject()
{
	for (auto Iter : ObjectPools)
	{
		Iter.Value->Clear();
		Iter.Value->ConditionalBeginDestroy();
	}
	ObjectPools.Empty();
}
