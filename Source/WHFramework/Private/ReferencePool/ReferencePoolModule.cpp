// Fill out your copyright notice in the Description page of Project Settings.


#include "ReferencePool/ReferencePoolModule.h"

#include "ReferencePool/ReferencePool.h"

// Sets default values
AReferencePoolModule::AReferencePoolModule()
{
	ModuleName = FName("ReferencePoolModule");

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
