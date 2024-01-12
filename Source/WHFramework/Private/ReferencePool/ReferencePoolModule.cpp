// Fill out your copyright notice in the Description page of Project Settings.


#include "ReferencePool/ReferencePoolModule.h"

#include "ReferencePool/ReferencePool.h"
		
IMPLEMENTATION_MODULE(UReferencePoolModule)

// Sets default values
UReferencePoolModule::UReferencePoolModule()
{
	ModuleName = FName("ReferencePoolModule");
	ModuleDisplayName = FText::FromString(TEXT("Reference Pool Module"));

	bModuleRequired = true;

	ReferencePools = TMap<TSubclassOf<UObject>, UReferencePool*>();
}

UReferencePoolModule::~UReferencePoolModule()
{
	TERMINATION_MODULE(UReferencePoolModule)
}

#if WITH_EDITOR
void UReferencePoolModule::OnGenerate()
{
	Super::OnGenerate();
}

void UReferencePoolModule::OnDestroy()
{
	Super::OnDestroy();

	TERMINATION_MODULE(UReferencePoolModule)
}
#endif

void UReferencePoolModule::OnInitialize()
{
	Super::OnInitialize();
}

void UReferencePoolModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
}

void UReferencePoolModule::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

void UReferencePoolModule::OnPause()
{
	Super::OnPause();
}

void UReferencePoolModule::OnUnPause()
{
	Super::OnUnPause();
}

void UReferencePoolModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);
}

void UReferencePoolModule::ClearAllReference()
{
	for (auto Iter : ReferencePools)
	{
		Iter.Value->Clear();
		Iter.Value->ConditionalBeginDestroy();
	}
	ReferencePools.Empty();
}
