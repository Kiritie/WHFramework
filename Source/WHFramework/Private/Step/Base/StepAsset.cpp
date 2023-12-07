// Copyright Epic Games, Inc. All Rights Reserved.

#include "Step/Base/StepAsset.h"
#include "AssetRegistry/AssetData.h"
#include "Step/Base/StepBase.h"

UStepAsset::UStepAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::GetEmpty();
	
	NativeClass = GetClass();

	RootSteps = TArray<UStepBase*>();
	StepMap = TMap<FString, UStepBase*>();

	FirstStep = nullptr;
}

void UStepAsset::Initialize(UAssetBase* InSource)
{
	Super::Initialize(InSource);

	for(const auto Iter : RootSteps)
	{
		if(!Iter) continue;

		Iter->OnInitialize();

		AAA(Iter, StepMap);
		
		if(!FirstStep)
		{
			FirstStep = Iter;
		}
	}
}

void UStepAsset::AAA(UStepBase* InStep, TMap<FString, UStepBase*>& InMap)
{
	InMap.Add(InStep->StepGUID, InStep);
	for(auto Iter : InStep->SubSteps)
	{
		AAA(Iter, InMap);
	}
};

#if WITH_EDITOR
void UStepAsset::GenerateStepListItem(TArray<TSharedPtr<FStepListItem>>& OutStepListItems)
{
	OutStepListItems = TArray<TSharedPtr<FStepListItem>>();
	for (int32 i = 0; i < RootSteps.Num(); i++)
	{
		auto Item = MakeShared<FStepListItem>();
		RootSteps[i]->GenerateListItem(Item);
		OutStepListItems.Add(Item);
	}
}

void UStepAsset::UpdateStepListItem(TArray<TSharedPtr<FStepListItem>>& OutStepListItems)
{
	for (int32 i = 0; i < RootSteps.Num(); i++)
	{
		RootSteps[i]->StepIndex = i;
		RootSteps[i]->StepHierarchy = 0;
		RootSteps[i]->UpdateListItem(OutStepListItems[i]);
	}
}

bool UStepAsset::CanAddStep(TSubclassOf<UStepBase> InStepClass)
{
	return true;
}

void UStepAsset::ClearAllStep()
{
	for(auto Iter : RootSteps)
	{
		if(Iter)
		{
			Iter->OnUnGenerate();
		}
	}
	
	RootSteps.Empty();

	StepMap.Empty();

	Modify();
}
#endif
