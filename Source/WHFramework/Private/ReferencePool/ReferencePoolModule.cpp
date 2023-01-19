// Fill out your copyright notice in the Description page of Project Settings.


#include "ReferencePool/ReferencePoolModule.h"

#include "ReferencePool/ReferencePool.h"
		
MODULE_INSTANCE_IMPLEMENTATION(AReferencePoolModule, false)

// Sets default values
AReferencePoolModule::AReferencePoolModule()
{
	ModuleName = FName("ReferencePoolModule");

	ReferencePools = TMap<TSubclassOf<UObject>, UReferencePool*>();
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

void AReferencePoolModule::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);
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
	ClearAllReference();
}

void AReferencePoolModule::ClearAllReference()
{
	for (auto Iter : ReferencePools)
	{
		Iter.Value->Clear();
		Iter.Value->ConditionalBeginDestroy();
	}
	ReferencePools.Empty();
}
