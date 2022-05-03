// Fill out your copyright notice in the Description page of Project Settings.


#include "ReferencePool/ReferencePoolModule.h"

#include "ReferencePool/ReferencePool.h"

// Sets default values
AReferencePoolModule::AReferencePoolModule()
{
	ModuleName = FName("ReferencePoolModule");

	Limit = 100;
	ReferencePools = TMap<UClass*, UReferencePool*>();
}

#if WITH_EDITOR
void AReferencePoolModule::OnGenerate_Implementation()
{
	Super::OnGenerate_Implementation();
}

void AReferencePoolModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
}
#endif

void AReferencePoolModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void AReferencePoolModule::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();
}

void AReferencePoolModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void AReferencePoolModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void AReferencePoolModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

void AReferencePoolModule::OnTermination_Implementation()
{
	Super::OnTermination_Implementation();
	ClearAllActor();
}

AActor* AReferencePoolModule::K2_SpawnReference(TSubclassOf<AActor> InType)
{
	return SpawnReference<AActor>(InType);
}

void AReferencePoolModule::DespawnReference(AActor* InActor)
{
	if(!InActor) return;

	UClass* TmpClass = InActor->GetClass();
	
	if(TmpClass->ImplementsInterface(UReferencePoolInterface::StaticClass()))
	{
		if (!ReferencePools.Contains(TmpClass))
		{
			UReferencePool* ReferencePool = NewObject<UReferencePool>(this);
			ReferencePool->Initialize(Limit, TmpClass);
			ReferencePools.Add(TmpClass, ReferencePool);
		}
		ReferencePools[TmpClass]->Despawn(InActor);
	}
}

void AReferencePoolModule::ClearAllActor()
{
	for (auto Iter : ReferencePools)
	{
		Iter.Value->Clear();
	}
	ReferencePools.Empty();
}
