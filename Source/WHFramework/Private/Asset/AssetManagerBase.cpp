// Copyright Epic Games, Inc. All Rights Reserved.

#include "Asset/AssetManagerBase.h"

#include "AbilitySystemGlobals.h"
#include "Ability/Item/AbilityItemDataBase.h"
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
		//UReferencePoolModuleBPLibrary::CreateReference<UAssetManagerBase>(This);
		return *This;
	}
	else
	{
		ensureEditor(true);
		WHLog(FString::Printf(TEXT("Invalid AssetManager in DefaultEngine.ini, must be AssetManagerBase!")), EDebugCategory::Asset, EDebugVerbosity::Warning);
		return UReferencePoolModuleBPLibrary::GetReference<UAssetManagerBase>();
	}
}

void UAssetManagerBase::StartInitialLoading()
{
	Super::StartInitialLoading();

	UAbilitySystemGlobals::Get().InitGlobalData();
}

TSharedPtr<FStreamableHandle> UAssetManagerBase::LoadPrimaryAsset(const FPrimaryAssetId& AssetToLoad, const TArray<FName>& LoadBundles, FStreamableDelegate DelegateToCall, TAsyncLoadPriority Priority)
{
	return Super::LoadPrimaryAsset(AssetToLoad, LoadBundles, DelegateToCall, Priority);
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
		WHLog(FString::Printf(TEXT("Failed to load item for identifier %s!"), *InPrimaryAssetId.ToString()), EDebugCategory::Asset, EDebugVerbosity::Warning);
	}
	return LoadedItem;
}

TSharedPtr<FStreamableHandle> UAssetManagerBase::LoadPrimaryAssets(const TArray<FPrimaryAssetId>& AssetsToLoad, const TArray<FName>& LoadBundles, FStreamableDelegate DelegateToCall, TAsyncLoadPriority Priority)
{
	return Super::LoadPrimaryAssets(AssetsToLoad, LoadBundles, DelegateToCall, Priority);
}

TArray<UPrimaryAssetBase*> UAssetManagerBase::LoadPrimaryAssets(FPrimaryAssetType InPrimaryAssetType, bool bLogWarning)
{
	TArray<UPrimaryAssetBase*> LoadedItems;
	
	TArray<FSoftObjectPath> ItemPaths;
	GetPrimaryAssetPathList(InPrimaryAssetType, ItemPaths);

	if(!PrimaryAssetsMap.Contains(InPrimaryAssetType))
	{
		for(auto Iter : ItemPaths)
		{
			if(UPrimaryAssetBase* LoadedItem = Cast<UPrimaryAssetBase>(Iter.TryLoad()))
			{
				LoadedItems.Add(LoadedItem);
				PrimaryAssetMap.Emplace(LoadedItem->GetPrimaryAssetId(), LoadedItem);
			}
		}
		if(LoadedItems.Num() > 0)
		{
			PrimaryAssetsMap.Add(InPrimaryAssetType, FPrimaryAssets(LoadedItems));
		}
	}
	else
	{
		LoadedItems = PrimaryAssetsMap[InPrimaryAssetType].Assets;
	}

	if(bLogWarning && LoadedItems.Num() == 0)
	{
		WHLog(FString::Printf(TEXT("Failed to load item for identifier %s!"), *InPrimaryAssetType.ToString()), EDebugCategory::Asset, EDebugVerbosity::Warning);
	}
	return LoadedItems;
}
