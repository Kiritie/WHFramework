// Copyright Epic Games, Inc. All Rights Reserved.

#include "Procedure/Base/ProcedureAsset.h"
#include "AssetRegistry/AssetData.h"
#include "Procedure/Base/ProcedureBase.h"

UProcedureAsset::UProcedureAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::GetEmpty();
	
	NativeClass = GetClass();

	Procedures = TArray<UProcedureBase*>();
	ProcedureMap = TMap<TSubclassOf<UProcedureBase>, UProcedureBase*>();

	FirstProcedure = nullptr;
}

void UProcedureAsset::Initialize(UAssetBase* InSource)
{
	Super::Initialize(InSource);

	if(!FirstProcedure && Procedures.Num() > 0)
	{
		FirstProcedure = Procedures[0];
	}
}

#if WITH_EDITOR
void UProcedureAsset::GenerateProcedureListItem(TArray<TSharedPtr<FProcedureListItem>>& OutProcedureListItems)
{
	OutProcedureListItems = TArray<TSharedPtr<FProcedureListItem>>();
	for (int32 i = 0; i < Procedures.Num(); i++)
	{
		auto Item = MakeShared<FProcedureListItem>();
		Procedures[i]->GenerateListItem(Item);
		OutProcedureListItems.Add(Item);
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
