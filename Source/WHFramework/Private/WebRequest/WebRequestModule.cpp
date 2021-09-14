// Fill out your copyright notice in the Description page of Project Settings.


#include "WebRequest/WebRequestModule.h"

// Sets default values
AWebRequestModule::AWebRequestModule()
{
	ModuleName = FName("WebRequestModule");
}

#if WITH_EDITOR
void AWebRequestModule::OnGenerate_Implementation()
{
	Super::OnGenerate_Implementation();
}

void AWebRequestModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
}
#endif

void AWebRequestModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void AWebRequestModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void AWebRequestModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void AWebRequestModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}
