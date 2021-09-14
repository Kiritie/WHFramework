// Fill out your copyright notice in the Description page of Project Settings.


#include "SpawnPool/SpawnPoolModule.h"

#include "SpawnPool/SpawnPool.h"

// Sets default values
ASpawnPoolModule::ASpawnPoolModule()
{
	ModuleName = FName("SpawnPoolModule");

	Limit = 100;
	SpawnPools = TMap<UClass*, USpawnPool*>();
}

#if WITH_EDITOR
void ASpawnPoolModule::OnGenerate_Implementation()
{
	Super::OnGenerate_Implementation();
}

void ASpawnPoolModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
}
#endif

void ASpawnPoolModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void ASpawnPoolModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void ASpawnPoolModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void ASpawnPoolModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

AActor* ASpawnPoolModule::K2_SpawnActor(TSubclassOf<AActor> InType)
{
	if(!InType) return nullptr;

	if (!SpawnPools.Contains(InType))
	{
		USpawnPool* SpawnPool = NewObject<USpawnPool>(this);
		SpawnPool->Initialize(Limit, InType);
		SpawnPools.Add(InType, SpawnPool);
	}
	return SpawnPools[InType]->Spawn();
}

void ASpawnPoolModule::DespawnActor(AActor* InActor)
{
	if(!InActor) return;

	UClass* TmpClass = InActor->GetClass();
	
	if (!SpawnPools.Contains(TmpClass))
	{
		USpawnPool* SpawnPool = NewObject<USpawnPool>(this);
		SpawnPool->Initialize(Limit, TmpClass);
		SpawnPools.Add(TmpClass, SpawnPool);
	}
	SpawnPools[TmpClass]->Despawn(InActor);
}

void ASpawnPoolModule::ClearAllActor()
{
	for (auto Iter : SpawnPools)
	{
		Iter.Value->Clear();
	}
	SpawnPools.Empty();
}
