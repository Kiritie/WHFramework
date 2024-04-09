// Fill out your copyright notice in the Description page of Project Settings.


#include "Debug/DebugModule.h"

#include "Common/CommonStatics.h"

IMPLEMENTATION_MODULE(UDebugModule)

// Sets default values
UDebugModule::UDebugModule()
{
	ModuleName = FName("DebugModule");
	ModuleDisplayName = FText::FromString(TEXT("Debug Module"));

	bModuleRequired = true;

	DebugCategoryStates = TMap<TEnumAsByte<EDebugCategory>, FDebugCategoryState>();
	DON_WITHINDEX((int32)EDC_Custom1, i, 
		DebugCategoryStates.Add((EDebugCategory)i, FDebugCategoryState(true, true));
	)
}

UDebugModule::~UDebugModule()
{
	TERMINATION_MODULE(UDebugModule)
}

#if WITH_EDITOR
void UDebugModule::OnGenerate()
{
	Super::OnGenerate();
}

void UDebugModule::OnDestroy()
{
	Super::OnDestroy();

	TERMINATION_MODULE(UDebugModule)
}
#endif

void UDebugModule::OnInitialize()
{
	Super::OnInitialize();

	for(auto Iter : DebugCategoryStates)
	{
		UDebugModuleStatics::SetDebugCategoryState(Iter.Key, Iter.Value);
	}
}

void UDebugModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
}

void UDebugModule::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

void UDebugModule::OnPause()
{
	Super::OnPause();
}

void UDebugModule::OnUnPause()
{
	Super::OnUnPause();
}

void UDebugModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);
}
