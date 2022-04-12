// Fill out your copyright notice in the Description page of Project Settings.


#include "Network/NetworkModule.h"

#include "Main/MainModule.h"
#include "Debug/DebugModuleTypes.h"
#include "Network/NetworkModuleBPLibrary.h"
#include "Network/NetworkModuleNetworkComponent.h"

// ParamSets default values
ANetworkModule::ANetworkModule()
{
	ModuleName = FName("NetworkModule");
}

#if WITH_EDITOR
void ANetworkModule::OnGenerate_Implementation()
{
	Super::OnGenerate_Implementation();
}

void ANetworkModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
}
#endif

void ANetworkModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void ANetworkModule::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();
}

void ANetworkModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void ANetworkModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void ANetworkModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}
