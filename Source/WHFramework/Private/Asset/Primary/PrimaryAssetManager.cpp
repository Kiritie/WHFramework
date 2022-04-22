// Copyright Epic Games, Inc. All Rights Reserved.

#include "Asset/Primary/PrimaryAssetManager.h"

#include "AbilitySystemGlobals.h"
#include "Ability/Item/ItemDataBase.h"
#include "Debug/DebugModuleTypes.h"

UPrimaryAssetManager::UPrimaryAssetManager()
{
	AssetMap = TMap<FPrimaryAssetId, UPrimaryAssetBase*>();
}

UPrimaryAssetManager& UPrimaryAssetManager::Get()
{
	UPrimaryAssetManager* This = Cast<UPrimaryAssetManager>(GEngine->AssetManager);

	if (This)
	{
		return *This;
	}
	else
	{
		WH_LOG(WH_Asset, Fatal, TEXT("Invalid AssetManager in DefaultEngine.ini, must be PrimaryAssetManager!"));
		return *NewObject<UPrimaryAssetManager>();
	}
}

void UPrimaryAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();

	UAbilitySystemGlobals::Get().InitGlobalData();
}

UPrimaryAssetBase* UPrimaryAssetManager::LoadAsset(const FPrimaryAssetId& InPrimaryAssetId, bool bLogWarning)
{
	UPrimaryAssetBase* LoadedItem = nullptr;

	if(!AssetMap.Contains(InPrimaryAssetId))
	{
		FSoftObjectPath ItemPath = GetPrimaryAssetPath(InPrimaryAssetId);
		LoadedItem = Cast<UPrimaryAssetBase>(ItemPath.TryLoad());
		if(LoadedItem)
		{
			AssetMap.Add(InPrimaryAssetId, LoadedItem);
		}
	}
	else
	{
		LoadedItem = AssetMap[InPrimaryAssetId];
	}

	if (bLogWarning && !LoadedItem)
	{
		WH_LOG(WH_Asset, Warning, TEXT("Failed to load item for identifier %s!"), *InPrimaryAssetId.ToString());
	}
	return LoadedItem;
}