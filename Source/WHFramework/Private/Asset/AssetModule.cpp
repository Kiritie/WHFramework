// Fill out your copyright notice in the Description page of Project Settings.


#include "Asset/AssetModule.h"
		
MODULE_INSTANCE_IMPLEMENTATION(AAssetModule)

// Sets default values
AAssetModule::AAssetModule()
{
	ModuleName = FName("AssetModule");

}

#if WITH_EDITOR
void AAssetModule::OnGenerate_Implementation()
{
	
}

void AAssetModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
}
#endif

void AAssetModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();

	for(auto Iter : DataAssets)
	{
		if(Iter)
		{
			DataAssetMap.Add(Iter->GetDataAssetName(), Iter);
		}
	}
	for(auto Iter : DataTables)
	{
		if(Iter)
		{
			DataTableMap.Add(Iter->RowStruct, Iter);
		}
	}
}

void AAssetModule::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();
}

void AAssetModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void AAssetModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void AAssetModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

bool AAssetModule::HasDataAsset(FName InDataAssetName) const
{
	return DataAssetMap.Contains(InDataAssetName);
}

UDataAssetBase* AAssetModule::GetDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass, FName InDataAssetName) const
{
	if(!InDataAssetClass) return nullptr;
	
	if(InDataAssetName.IsNone()) InDataAssetName = InDataAssetClass.GetDefaultObject()->GetDataAssetName();

	return GetDataAsset<UDataAssetBase>(InDataAssetName);
}

UDataAssetBase* AAssetModule::CreateDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass, FName InDataAssetName)
{
	if(!InDataAssetClass) return nullptr;

	if(InDataAssetName.IsNone()) InDataAssetName = InDataAssetClass.GetDefaultObject()->GetDataAssetName();

	return CreateDataAsset<UDataAssetBase>(InDataAssetName);
}

bool AAssetModule::RemoveDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass, FName InDataAssetName)
{
	if(!InDataAssetClass) return false;

	if(InDataAssetName.IsNone()) InDataAssetName = InDataAssetClass.GetDefaultObject()->GetDataAssetName();

	return RemoveDataAsset<UDataAssetBase>(InDataAssetName);
}

void AAssetModule::RemoveAllDataAsset()
{
	for (auto Iter : DataAssetMap)
	{
		if(!Iter.Value) continue;
		Iter.Value->ConditionalBeginDestroy();
	}
	DataAssetMap.Empty();
}

bool AAssetModule::AddDataTable(UDataTable* InDataTable)
{
	if(!InDataTable) return false;
		
	if(!DataTableMap.Contains(InDataTable->RowStruct))
	{
		DataTableMap.Add(InDataTable->RowStruct, InDataTable);
		return true;
	}
	return false;
}

bool AAssetModule::RemoveDataTable(UDataTable* InDataTable)
{
	if(!InDataTable) return false;
		
	if(DataTableMap.Contains(InDataTable->RowStruct))
	{
		DataTableMap.Remove(InDataTable->RowStruct);
		return true;
	}
	return false;
}

void AAssetModule::RemoveAllDataTable()
{
	DataTableMap.Empty();
}
