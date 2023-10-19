// Fill out your copyright notice in the Description page of Project Settings.


#include "Debug/DebugModule.h"

#include "Common/CommonBPLibrary.h"

IMPLEMENTATION_MODULE(ADebugModule)

// Sets default values
ADebugModule::ADebugModule()
{
	ModuleName = FName("DebugModule");

	DebugCategoryStates = TMap<TEnumAsByte<EDebugCategory>, FDebugCategoryState>();
	DON_WITHINDEX(FindObject<UEnum>(nullptr, TEXT("/Script/WHFramework.EDebugCategory"))->NumEnums() - 2, i, 
		DebugCategoryStates.Add((EDebugCategory)i, FDebugCategoryState(true, true));
	)
}

ADebugModule::~ADebugModule()
{
	TERMINATION_MODULE(ADebugModule)
}

#if WITH_EDITOR
void ADebugModule::OnGenerate()
{
	Super::OnGenerate();
}

void ADebugModule::OnDestroy()
{
	Super::OnDestroy();
}
#endif

void ADebugModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void ADebugModule::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);
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

void ADebugModule::OnTermination_Implementation(EPhase InPhase)
{
	Super::OnTermination_Implementation(InPhase);
}

FDebugCategoryState ADebugModule::GetDebugCategoryState(EDebugCategory InCategory) const
{
	if(DebugCategoryStates.Contains(InCategory))
	{
		return DebugCategoryStates[InCategory];
	}
	return FDebugCategoryState();
}

void ADebugModule::SetDebugCategoryState(EDebugCategory InCategory, FDebugCategoryState InState)
{
	if(DebugCategoryStates.Contains(InCategory))
	{
		DebugCategoryStates[InCategory] = InState;
	}
}
