// Copyright Epic Games, Inc. All Rights Reserved.

#include "Asset/Primary/PrimaryAssetManager.h"

#include "AbilitySystemGlobals.h"
#include "Asset/Primary/Item/ItemAssetBase.h"
#include "Debug/DebugModuleTypes.h"

UPrimaryAssetManager::UPrimaryAssetManager()
{
	UItemAssetBase::Empty = NewObject<UItemAssetBase>();
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
		WH_LOG(WHAsset, Fatal, TEXT("Invalid AssetManager in DefaultEngine.ini, must be ItemAssetManager!"));
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
	FSoftObjectPath ItemPath = GetPrimaryAssetPath(InPrimaryAssetId);

	UPrimaryAssetBase* LoadedItem = Cast<UPrimaryAssetBase>(ItemPath.TryLoad());

	if (bLogWarning && LoadedItem == nullptr)
	{
		WH_LOG(WHAsset, Warning, TEXT("Failed to load item for identifier %s!"), *InPrimaryAssetId.ToString());
	}
	return LoadedItem ? LoadedItem : UPrimaryAssetBase::Empty;
}