// Fill out your copyright notice in the Description page of Project Settings.


#include "Debug/DebugModule.h"

// Sets default values
ADebugModule::ADebugModule()
{
	ModuleName = FName("DebugModule");

}

#if WITH_EDITOR
void ADebugModule::OnGenerate_Implementation()
{
	Super::OnGenerate_Implementation();
}

void ADebugModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
}
#endif

void ADebugModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void ADebugModule::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();
}

void ADebugModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void ADebugModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void ADebugModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}
