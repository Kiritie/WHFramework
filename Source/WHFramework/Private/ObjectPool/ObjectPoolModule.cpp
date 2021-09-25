// Fill out your copyright notice in the Description page of Project Settings.


#include "ObjectPool/ObjectPoolModule.h"

#include "ObjectPool/ObjectPool.h"

// Sets default values
AObjectPoolModule::AObjectPoolModule()
{
	ModuleName = FName("ObjectPoolModule");

	Limit = 100;
	ObjectPools = TMap<UClass*, UObjectPool*>();
}

#if WITH_EDITOR
void AObjectPoolModule::OnGenerate_Implementation()
{
	Super::OnGenerate_Implementation();
}

void AObjectPoolModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
}
#endif

void AObjectPoolModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
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

UObject* AObjectPoolModule::K2_SpawnObject(TSubclassOf<UObject> InType)
{
	return SpawnObject<UObject>(InType);
}

void AObjectPoolModule::DespawnObject(UObject* InObject)
{
	if(!InObject) return;

	UClass* TmpClass = InObject->GetClass();
	
	if (!ObjectPools.Contains(TmpClass))
	{
		UObjectPool* ObjectPool = NewObject<UObjectPool>(this);
		ObjectPool->Initialize(Limit, TmpClass);
		ObjectPools.Add(TmpClass, ObjectPool);
	}
	ObjectPools[TmpClass]->Despawn(InObject);
}

void AObjectPoolModule::ClearAllObject()
{
	for (auto Iter : ObjectPools)
	{
		Iter.Value->Clear();
	}
	ObjectPools.Empty();
}
