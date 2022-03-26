// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/AbilityModule.h"

// Sets default values
AAbilityModule::AAbilityModule()
{
	ModuleName = FName("AbilityModule");
}

#if WITH_EDITOR
void AAbilityModule::OnGenerate_Implementation()
{
	Super::OnGenerate_Implementation();
}

void AAbilityModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
}
#endif

void AAbilityModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void AAbilityModule::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();
}

void AAbilityModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void AAbilityModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void AAbilityModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}
