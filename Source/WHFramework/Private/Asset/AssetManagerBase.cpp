// Copyright Epic Games, Inc. All Rights Reserved.

#include "Asset/AssetManagerBase.h"

#include "AbilitySystemGlobals.h"
#include "Debug/DebugModuleTypes.h"

UAssetManagerBase::UAssetManagerBase()
{
	PrimaryAssetMap = TMap<FPrimaryAssetType, FPrimaryAssets>();
}

UAssetManagerBase& UAssetManagerBase::Get()
{
	if(UAssetManagerBase* Instance = Cast<UAssetManagerBase>(GEngine->AssetManager))
	{
		return *Instance;
	}
	ensureEditor(true);
	WHLog(FString::Printf(TEXT("Invalid AssetManager in DefaultEngine.ini, must be AssetManagerBase!")), EDebugCategory::Asset, EDebugVerbosity::Warning);
	return UReferencePoolModuleBPLibrary::GetReference<UAssetManagerBase>();
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
	UPrimaryAssetBase* LoadedAsset;

	if(!PrimaryAssetMap.FindOrAdd(InPrimaryAssetId.PrimaryAssetType).Assets.Contains(InPrimaryAssetId))
	{
		const FSoftObjectPath AssetPath = GetPrimaryAssetPath(InPrimaryAssetId);
		LoadedAsset = Cast<UPrimaryAssetBase>(AssetPath.TryLoad());
		if(LoadedAsset)
		{
			PrimaryAssetMap[InPrimaryAssetId.PrimaryAssetType].Assets.Add(InPrimaryAssetId, LoadedAsset);
		}
	}
	else
	{
		LoadedAsset = PrimaryAssetMap[InPrimaryAssetId.PrimaryAssetType].Assets[InPrimaryAssetId];
	}

	if(bLogWarning && !LoadedAsset)
	{
		WHLog(FString::Printf(TEXT("Failed to load asset for identifier %s!"), *InPrimaryAssetId.ToString()), EDebugCategory::Asset, EDebugVerbosity::Warning);
	}
	return LoadedAsset;
}

TSharedPtr<FStreamableHandle> UAssetManagerBase::LoadPrimaryAssets(const TArray<FPrimaryAssetId>& AssetsToLoad, const TArray<FName>& LoadBundles, FStreamableDelegate DelegateToCall, TAsyncLoadPriority Priority)
{
	return Super::LoadPrimaryAssets(AssetsToLoad, LoadBundles, DelegateToCall, Priority);
}

TArray<UPrimaryAssetBase*> UAssetManagerBase::LoadPrimaryAssets(FPrimaryAssetType InPrimaryAssetType, bool bLogWarning)
{
	TArray<UPrimaryAssetBase*> LoadedAssets;
	if(!PrimaryAssetMap.Contains(InPrimaryAssetType))
	{
		TArray<FSoftObjectPath> AssetPaths;
		GetPrimaryAssetPathList(InPrimaryAssetType, AssetPaths);
		TMap<FPrimaryAssetId, UPrimaryAssetBase*> LoadedAssetMap;
		for(auto Iter : AssetPaths)
		{
			if(UPrimaryAssetBase* LoadedAsset = Cast<UPrimaryAssetBase>(Iter.TryLoad()))
			{
				LoadedAssets.Add(LoadedAsset);
				LoadedAssetMap.Add(LoadedAsset->GetPrimaryAssetId(), LoadedAsset);
			}
		}
		PrimaryAssetMap.Add(InPrimaryAssetType, LoadedAssetMap);
	}
	else
	{
		PrimaryAssetMap[InPrimaryAssetType].Assets.GenerateValueArray(LoadedAssets);
	}

	if(bLogWarning && LoadedAssets.Num() == 0)
	{
		WHLog(FString::Printf(TEXT("Failed to load asset for identifier %s!"), *InPrimaryAssetType.ToString()), EDebugCategory::Asset, EDebugVerbosity::Warning);
	}
	return LoadedAssets;
}
