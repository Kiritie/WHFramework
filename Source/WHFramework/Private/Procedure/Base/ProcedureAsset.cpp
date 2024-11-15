// Copyright Epic Games, Inc. All Rights Reserved.

#include "Procedure/Base/ProcedureAsset.h"
#include "AssetRegistry/AssetData.h"
#include "Procedure/Base/ProcedureBase.h"

UProcedureAsset::UProcedureAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::GetEmpty();
	
	NativeClass = GetClass();

	bAutoSwitchFirst = false;
	FirstProcedure = nullptr;

	Procedures = TArray<UProcedureBase*>();
	ProcedureMap = TMap<TSubclassOf<UProcedureBase>, UProcedureBase*>();
}

void UProcedureAsset::Initialize(UAssetBase* InSource)
{
	Super::Initialize(InSource);

	for(const auto Iter : Procedures)
	{
		if(!Iter) continue;

		Iter->OnInitialize();

		ProcedureMap.Add(Iter->GetClass(), Iter);
		
		if(!FirstProcedure)
		{
			FirstProcedure = Iter;
		}
	}
}

#if WITH_EDITOR
void UProcedureAsset::GenerateProcedureListItem(TArray<TSharedPtr<FProcedureListItem>>& OutProcedureListItems, const FString& InFilterText)
{
	OutProcedureListItems = TArray<TSharedPtr<FProcedureListItem>>();
	for (int32 i = 0; i < Procedures.Num(); i++)
	{
		auto Item = MakeShared<FProcedureListItem>();
		if(Procedures[i]->GenerateListItem(Item, InFilterText))
		{
			OutProcedureListItems.Add(Item);
		}
	}
}

void UProcedureAsset::UpdateProcedureListItem(TArray<TSharedPtr<FProcedureListItem>>& OutProcedureListItems)
{
	for (int32 i = 0; i < Procedures.Num(); i++)
	{
		Procedures[i]->ProcedureIndex = i;
		Procedures[i]->UpdateListItem(OutProcedureListItems[i]);
	}
}

bool UProcedureAsset::CanAddProcedure(TSubclassOf<UProcedureBase> InProcedureClass)
{
	for(auto Iter : Procedures)
	{
		if(Iter->GetClass() == InProcedureClass)
		{
			return false;
		}
	}
	return true;
}

void UProcedureAsset::ClearAllProcedure()
{
	for(auto Iter : Procedures)
	{
		if(Iter)
		{
			Iter->OnUnGenerate();
			Iter->ConditionalBeginDestroy();
		}
	}
	
	Procedures.Empty();

	ProcedureMap.Empty();

	Modify();
}
#endif
