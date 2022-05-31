// Copyright Epic Games, Inc. All Rights Reserved.

#include "Asset/AssetManagerBase.h"

#include "AbilitySystemGlobals.h"
#include "Ability/Item/ItemDataBase.h"
#include "Debug/DebugModuleTypes.h"

UAssetManagerBase::UAssetManagerBase()
{
	PrimaryAssetMap = TMap<FPrimaryAssetId, UPrimaryAssetBase*>();
}

UAssetManagerBase& UAssetManagerBase::Get()
{
	UAssetManagerBase* This = Cast<UAssetManagerBase>(GEngine->AssetManager);

	if(This)
	{
		UReferencePoolModuleBPLibrary::SetReference<UAssetManagerBase>(This);
		return *This;
	}
	else
	{
		ensureEditor(true);
		WHLog(WH_Asset, Warning, TEXT("Invalid AssetManager in DefaultEngine.ini, must be AssetManagerBase!"));
		return UReferencePoolModuleBPLibrary::GetReference<UAssetManagerBase>();
	}
}

TSharedPtr<FStreamableHandle> UAssetManagerBase::LoadPrimaryAsset(const FPrimaryAssetId& AssetToLoad, const TArray<FName>& LoadBundles, FStreamableDelegate DelegateToCall, TAsyncLoadPriority Priority)
{
	return Super::LoadPrimaryAsset(AssetToLoad, LoadBundles, DelegateToCall, Priority);
}

void UAssetManagerBase::StartInitialLoading()
{
	Super::StartInitialLoading();

	UAbilitySystemGlobals::Get().InitGlobalData();
}

UPrimaryAssetBase* UAssetManagerBase::LoadPrimaryAsset(const FPrimaryAssetId& InPrimaryAssetId, bool bLogWarning)
{
	UPrimaryAssetBase* LoadedItem;

	if(!PrimaryAssetMap.Contains(InPrimaryAssetId))
	{
		FSoftObjectPath ItemPath = GetPrimaryAssetPath(InPrimaryAssetId);
		LoadedItem = Cast<UPrimaryAssetBase>(ItemPath.TryLoad());
		if(LoadedItem)
		{
			PrimaryAssetMap.Add(InPrimaryAssetId, LoadedItem);
		}
	}
	else
	{
		LoadedItem = PrimaryAssetMap[InPrimaryAssetId];
	}

	if(bLogWarning && !LoadedItem)
	{
		WHLog(WH_Asset, Warning, TEXT("Failed to load item for identifier %s!"), *InPrimaryAssetId.ToString());
	}
	return LoadedItem;
}
