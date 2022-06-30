// Fill out your copyright notice in the Description page of Project Settings.


#include "FSM/FSMModule.h"

// ParamSets default values
AFSMModule::AFSMModule()
{
	ModuleName = FName("FSMModule");
}

#if WITH_EDITOR
void AFSMModule::OnGenerate_Implementation()
{
	Super::OnGenerate_Implementation();
}

void AFSMModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
}
#endif

void AFSMModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void AFSMModule::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();
}

void AFSMModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void AFSMModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void AFSMModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

void AFSMModule::OnTermination_Implementation()
{
	Super::OnTermination_Implementation();
}
