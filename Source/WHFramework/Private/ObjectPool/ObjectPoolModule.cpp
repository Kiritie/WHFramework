// Fill out your copyright notice in the Description page of Project Settings.


#include "ObjectPool/ObjectPoolModule.h"

#include "ObjectPool/ObjectPool.h"
#include "ObjectPool/Actor/ActorPool.h"
#include "ObjectPool/Widget/WidgetPool.h"

IMPLEMENTATION_MODULE(UObjectPoolModule)

// Sets default values
UObjectPoolModule::UObjectPoolModule()
{
	ModuleName = FName("ObjectPoolModule");
	ModuleDisplayName = FText::FromString(TEXT("Object Pool Module"));

	bModuleRequired = true;

	ObjectPools = TMap<TSubclassOf<UObject>, UObjectPool*>();
}

UObjectPoolModule::~UObjectPoolModule()
{
	TERMINATION_MODULE(UObjectPoolModule)
}

#if WITH_EDITOR
void UObjectPoolModule::OnGenerate()
{
	Super::OnGenerate();
}

void UObjectPoolModule::OnDestroy()
{
	Super::OnDestroy();

	TERMINATION_MODULE(UObjectPoolModule)
}
#endif

void UObjectPoolModule::OnInitialize()
{
	Super::OnInitialize();
}

void UObjectPoolModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
}

void UObjectPoolModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);
}

void UObjectPoolModule::OnPause()
{
	Super::OnPause();
}

void UObjectPoolModule::OnUnPause()
{
	Super::OnUnPause();
}

void UObjectPoolModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);

	if(PHASEC(InPhase, EPhase::Final))
	{
		ClearAllObject();
	}
}

FString UObjectPoolModule::GetModuleDebugMessage()
{
	FString DebugMessage;
	for(auto Iter : ObjectPools)
	{
		if(Iter.Value->GetCount() > 0)
		{
			DebugMessage.Appendf(TEXT("%s: %d\n"), *Iter.Key->GetName(), Iter.Value->GetCount());
		}
	}
	DebugMessage.RemoveFromEnd(TEXT("\n"));
	if(!DebugMessage.IsEmpty())
	{
		return DebugMessage;
	}
	return Super::GetModuleDebugMessage();
}

bool UObjectPoolModule::HasPool(TSubclassOf<UObject> InType) const
{
	return ObjectPools.Contains(InType);
}

UObjectPool* UObjectPoolModule::GetPool(TSubclassOf<UObject> InType) const
{
	if(HasPool(InType))
	{
		return ObjectPools[InType];
	}
	return nullptr;
}

UObjectPool* UObjectPoolModule::CreatePool(TSubclassOf<UObject> InType)
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
	ObjectPool->Initialize(IObjectPoolInterface::Execute_GetLimit(InType.GetDefaultObject()), InType);
	ObjectPools.Add(InType, ObjectPool);
	return ObjectPool;
}

void UObjectPoolModule::DestroyPool(TSubclassOf<UObject> InType)
{
	if(!InType || !HasPool(InType)) return;

	ObjectPools[InType]->ConditionalBeginDestroy();
	ObjectPools.Remove(InType);
}

bool UObjectPoolModule::HasObject(TSubclassOf<UObject> InType)
{
	if(!InType || !InType->ImplementsInterface(UObjectPoolInterface::StaticClass())) return false;

	if(ObjectPools.Contains(InType))
	{
		return ObjectPools[InType]->GetCount() > 0;
	}
	return false;
}

UObject* UObjectPoolModule::SpawnObject(TSubclassOf<UObject> InType, UObject* InOwner, const TArray<FParameter>& InParams)
{
	if(!InType || !InType->ImplementsInterface(UObjectPoolInterface::StaticClass())) return nullptr;

	UObjectPool* ObjectPool = HasPool(InType) ? GetPool(InType) : CreatePool(InType);
	return ObjectPool->Spawn(InOwner, InParams);
}

void UObjectPoolModule::DespawnObject(UObject* InObject, bool bRecovery)
{
	if(!InObject || ModuleState == EModuleState::Terminated) return;

	UClass* InType = InObject->GetClass();
	if(!InType->ImplementsInterface(UObjectPoolInterface::StaticClass())) return;

	UObjectPool* ObjectPool = HasPool(InType) ? GetPool(InType) : CreatePool(InType);
	ObjectPool->Despawn(InObject, bRecovery);
}

void UObjectPoolModule::DespawnObjects(TArray<UObject*> InObjects, bool bRecovery)
{
	for(auto Iter : InObjects)
	{
		DespawnObject(Iter, bRecovery);
	}
}

void UObjectPoolModule::ClearObject(TSubclassOf<UObject> InType)
{
	if(!InType || !InType->ImplementsInterface(UObjectPoolInterface::StaticClass())) return;

	if(HasPool(InType))
	{
		GetPool(InType)->Clear();
	}
}

void UObjectPoolModule::ClearAllObject()
{
	for (auto Iter : ObjectPools)
	{
		Iter.Value->Clear();
	}
	ObjectPools.Empty();
}
