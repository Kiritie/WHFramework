// Copyright Epic Games, Inc. All Rights Reserved.

#include "Step/Base/StepAsset.h"
#include "AssetRegistry/AssetData.h"
#include "Step/Base/StepBase.h"

UStepAsset::UStepAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::GetEmpty();
	
	NativeClass = GetClass();

	bAutoStartFirst = false;
	FirstStep = nullptr;

	RootSteps = TArray<UStepBase*>();
	StepMap = TMap<FString, UStepBase*>();
}

void UStepAsset::Initialize()
{
	Super::Initialize();

	for(const auto Iter : RootSteps)
	{
		if(!Iter) continue;

		RecursiveArrayItems<UStepBase>(Iter, [this](const UStepBase* Step)
		{
			StepMap.Add(Step->StepGUID, const_cast<UStepBase*>(Step));
			return Step->SubSteps;
		});

		Iter->OnInitialize();
		
		if(!FirstStep)
		{
			FirstStep = Iter;
		}
	}
}

#if WITH_EDITOR
void UStepAsset::GenerateStepListItem(TArray<TSharedPtr<FStepListItem>>& OutStepListItems, const FString& InFilterText)
{
	OutStepListItems = TArray<TSharedPtr<FStepListItem>>();
	for (int32 i = 0; i < RootSteps.Num(); i++)
	{
		auto Item = MakeShared<FStepListItem>();
		if(RootSteps[i]->GenerateListItem(Item, InFilterText))
		{
			OutStepListItems.Add(Item);
		}
	}
}

void UStepAsset::UpdateStepListItem(TArray<TSharedPtr<FStepListItem>>& OutStepListItems)
{
	for (int32 i = 0; i < RootSteps.Num(); i++)
	{
		RootSteps[i]->StepIndex = i;
		RootSteps[i]->StepHierarchy = 0;
		if(OutStepListItems.IsValidIndex(i))
		{
			RootSteps[i]->UpdateListItem(OutStepListItems[i]);
		}
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
