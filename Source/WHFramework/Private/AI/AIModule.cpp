// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/AIModule.h"

#include "AI/AIModuleNetworkComponent.h"

IMPLEMENTATION_MODULE(UAIModule)

// Sets default values
UAIModule::UAIModule()
{
	ModuleName = FName("AIModule");
	ModuleDisplayName = FText::FromString(TEXT("AI Module"));

	ModuleNetworkComponent = UAIModuleNetworkComponent::StaticClass();
}

UAIModule::~UAIModule()
{
	TERMINATION_MODULE(UAIModule)
}

#if WITH_EDITOR
void UAIModule::OnGenerate()
{
	Super::OnGenerate();
}

void UAIModule::OnDestroy()
{
	Super::OnDestroy();
	
	TERMINATION_MODULE(UAIModule)
}
#endif

void UAIModule::OnInitialize()
{
	Super::OnInitialize();
}

void UAIModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
}

void UAIModule::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

void UAIModule::OnPause()
{
	Super::OnPause();
}

void UAIModule::OnUnPause()
{
	Super::OnUnPause();
}

void UAIModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);
}

void UAIModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}
