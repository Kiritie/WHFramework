// Fill out your copyright notice in the Description page of Project Settings.


#include "ReferencePool/ReferencePoolModule.h"

#include "ReferencePool/ReferencePool.h"
		
IMPLEMENTATION_MODULE(AReferencePoolModule)

// Sets default values
AReferencePoolModule::AReferencePoolModule()
{
	ModuleName = FName("ReferencePoolModule");

	ReferencePools = TMap<TSubclassOf<UObject>, UReferencePool*>();
}

AReferencePoolModule::~AReferencePoolModule()
{
	TERMINATION_MODULE(AReferencePoolModule)
}

#if WITH_EDITOR
void AReferencePoolModule::OnGenerate()
{
	Super::OnGenerate();
}

void AReferencePoolModule::OnDestroy()
{
	Super::OnDestroy();
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
