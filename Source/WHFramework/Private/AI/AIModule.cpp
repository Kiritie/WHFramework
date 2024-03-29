// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/AIModule.h"

IMPLEMENTATION_MODULE(AAIModule)

// Sets default values
AAIModule::AAIModule()
{
	ModuleName = FName("AIModule");
}

AAIModule::~AAIModule()
{
	TERMINATION_MODULE(AAIModule)
}

#if WITH_EDITOR
void AAIModule::OnGenerate()
{
	Super::OnGenerate();
}

void AAIModule::OnDestroy()
{
	Super::OnDestroy();
}
#endif

void AAIModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void AAIModule::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);
}

void AAIModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void AAIModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void AAIModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

void AAIModule::OnTermination_Implementation(EPhase InPhase)
{
	Super::OnTermination_Implementation(InPhase);
}

void AAIModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}
