// Fill out your copyright notice in the Description page of Project Settings.


#include "Asset/AssetModule.h"

#include "Asset/DataAsset/DataAssetBase.h"

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

bool AAssetModule::K2_HasDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass) const
{
	return HasDataAsset<UDataAssetBase>(InDataAssetClass);
}

UDataAssetBase* AAssetModule::K2_GetDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass) const
{
	return GetDataAsset<UDataAssetBase>(InDataAssetClass);
}

UDataAssetBase* AAssetModule::K2_GetDataAssetByName(FName InName, TSubclassOf<UDataAssetBase> InDataAssetClass) const
{
	if(DataAssetMap.Contains(InName))
	{
		return DataAssetMap[InName];
	}
	return nullptr;
}

UDataAssetBase* AAssetModule::K2_CreateDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass)
{
	return CreateDataAsset<UDataAssetBase>(InDataAssetClass);
}

bool AAssetModule::K2_RemoveDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass)
{
	return RemoveDataAsset<UDataAssetBase>(InDataAssetClass);
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
