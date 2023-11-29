// Copyright Epic Games, Inc. All Rights Reserved.

#include "Asset/AssetManagerBase.h"

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
	ensureEditorMsgf(false, FString::Printf(TEXT("Invalid AssetManager in DefaultEngine.ini, must be AssetManagerBase!")), EDC_Asset, EDV_Error);
	return *NewObject<UAssetManagerBase>();
}

void UAssetManagerBase::StartInitialLoading()
{
	Super::StartInitialLoading();
}

void UAssetManagerBase::RegisterPrimaryAssetType(FPrimaryAssetType InPrimaryAssetType)
{
	if(!PrimaryAssetMap.Contains(InPrimaryAssetType))
	{
		PrimaryAssetMap.Add(InPrimaryAssetType);
	}
}

TSharedPtr<FStreamableHandle> UAssetManagerBase::LoadPrimaryAsset(const FPrimaryAssetId& AssetToLoad, const TArray<FName>& LoadBundles, FStreamableDelegate DelegateToCall, TAsyncLoadPriority Priority)
{
	return Super::LoadPrimaryAsset(AssetToLoad, LoadBundles, DelegateToCall, Priority);
}

UPrimaryAssetBase* UAssetManagerBase::LoadPrimaryAsset(const FPrimaryAssetId& InPrimaryAssetId, bool bEnsured)
{
	UPrimaryAssetBase* LoadedAsset = nullptr;

	if(InPrimaryAssetId.IsValid() && PrimaryAssetMap.Contains(InPrimaryAssetId.PrimaryAssetType))
	{
		if(!PrimaryAssetMap[InPrimaryAssetId.PrimaryAssetType].Assets.Contains(InPrimaryAssetId))
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
	}

	if(bEnsured && !LoadedAsset)
	{
		WHLog(FString::Printf(TEXT("Failed to load asset for identifier %s!"), *InPrimaryAssetId.ToString()), EDC_Asset, EDV_Warning);
	}
	return LoadedAsset;
}

TSharedPtr<FStreamableHandle> UAssetManagerBase::LoadPrimaryAssets(const TArray<FPrimaryAssetId>& AssetsToLoad, const TArray<FName>& LoadBundles, FStreamableDelegate DelegateToCall, TAsyncLoadPriority Priority)
{
	return Super::LoadPrimaryAssets(AssetsToLoad, LoadBundles, DelegateToCall, Priority);
}

TArray<UPrimaryAssetBase*> UAssetManagerBase::LoadPrimaryAssets(FPrimaryAssetType InPrimaryAssetType, bool bEnsured)
{
	TArray<UPrimaryAssetBase*> LoadedAssets;

	if(PrimaryAssetMap.Contains(InPrimaryAssetType))
	{
		TArray<FPrimaryAssetId> AssetIds;
		GetPrimaryAssetIdList(InPrimaryAssetType, AssetIds);
	
		for(auto& Iter : AssetIds)
		{
			if(UPrimaryAssetBase* Asset = LoadPrimaryAsset(Iter, bEnsured))
			{
				LoadedAssets.Add(Asset);
			}
		}
	}
	
	if(bEnsured && LoadedAssets.IsEmpty())
	{
		WHLog(FString::Printf(TEXT("Failed to load assets for identifier %s!"), *InPrimaryAssetType.ToString()), EDC_Asset, EDV_Warning);
	}
	return LoadedAssets;
}

void UAssetManagerBase::ReleaseRuntimeData()
{
	for(auto& Iter1 : PrimaryAssetMap)
	{
		for(auto& Iter2 : Iter1.Value.Assets)
		{
			Iter2.Value->ResetData();
		}
	}
}
