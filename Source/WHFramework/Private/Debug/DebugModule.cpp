// Fill out your copyright notice in the Description page of Project Settings.


#include "Debug/DebugModule.h"

#include "Common/CommonStatics.h"
#include "Debug/DebugManager.h"

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
		FDebugManager::Get().SetDebugCategoryState(Iter.Key, Iter.Value);
	}
}

void UDebugModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
}

void UDebugModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);
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

#if WITH_EDITOR
void UDebugModule::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FProperty* Property = PropertyChangedEvent.MemberProperty;

	if(Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		const FName PropertyName = Property->GetFName();
		
		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UDebugModule, DebugCategoryStates))
		{
			for(auto Iter : DebugCategoryStates)
			{
				FDebugManager::Get().SetDebugCategoryState(Iter.Key, Iter.Value);
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

FDebugCategoryState UDebugModule::GetDebugCategoryState(EDebugCategory InCategory) const
{
	if(DebugCategoryStates.Contains(InCategory))
	{
		return DebugCategoryStates[InCategory];
	}
	return FDebugCategoryState();
}

void UDebugModule::SetDebugCategoryState(EDebugCategory InCategory, FDebugCategoryState InState)
{
	DebugCategoryStates.Emplace(InCategory, InState);

	FDebugManager::Get().SetDebugCategoryState(InCategory, InState);
}
