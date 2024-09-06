// Copyright Epic Games, Inc. All Rights Reserved.

#include "Asset/AssetManager.h"

#include "WHFrameworkCoreStatics.h"
#include "Debug/DebugTypes.h"
#include "Main/MainManager.h"

FUniqueType FAssetManager::Type = FUniqueType(&FManagerBase::Type);

IMPLEMENTATION_MANAGER(FAssetManager)

FAssetManager::FAssetManager() : FManagerBase(Type)
{
	AssetMap = TMap<FUniqueAssetID, FUniqueAssetData*>();
#if WITH_ENGINE
	TextureMap = TMap<FString, UTexture2D*>();
#endif
}

void FAssetManager::OnInitialize()
{
	FManagerBase::OnInitialize();
}

bool FAssetManager::RegisterAsset(FUniqueAssetData* InAssetData)
{
	if (!AssetMap.Contains(InAssetData->GetID()))
	{
		AssetMap.Add(InAssetData->GetID(), InAssetData);
		return true;
	}
	delete InAssetData;
	return false;
}

void FAssetManager::RegisterAssets(TArray<FUniqueAssetData*> InAssetDatas)
{
	for (const auto Iter : InAssetDatas)
	{
		RegisterAsset(Iter);
	}
}

bool FAssetManager::UnRegisterAssetByID(const FUniqueAssetID& InAssetID)
{
	if (AssetMap.Contains(InAssetID))
	{
		delete AssetMap[InAssetID];
		AssetMap.Remove(InAssetID);
		return true;
	}
	return false;
}

bool FAssetManager::UnRegisterAsset(FUniqueAssetData* InAssetData)
{
	return UnRegisterAssetByID(InAssetData->GetID());
}

void FAssetManager::UnRegisterAssets(TArray<FUniqueAssetData*> InAssetDatas)
{
	for (const auto Iter : InAssetDatas)
	{
		UnRegisterAsset(Iter);
	}
}

bool FAssetManager::ContainsAssetByID(const FUniqueAssetID& InAssetID) const
{
	return AssetMap.Contains(InAssetID);
}

bool FAssetManager::ContainsAssetByCondition(const TFunction<bool(FUniqueAssetData*)>& InCondition) const
{
	for (auto& Iter : AssetMap)
	{
		if (InCondition(Iter.Value))
		{
			return true;
		}
	}
	return false;
}

bool FAssetManager::ContainsAssetsByType(FUniqueType InAssetType) const
{
	for (auto& Iter : AssetMap)
	{
		if (Iter.Value->GetType().IsA(InAssetType))
		{
			return true;
		}
	}
	return false;
}

FUniqueAssetData* FAssetManager::LoadAssetByID(const FUniqueAssetID& InAssetID, bool bEnsured)
{
	FUniqueAssetData* LoadedAsset = nullptr;

	if (InAssetID.IsValid() && AssetMap.Contains(InAssetID))
	{
		LoadedAsset = AssetMap[InAssetID];
	}

	ensureEditorMsgf(!bEnsured || LoadedAsset, FString::Printf(TEXT("Failed to load asset for identifier %s!"), *InAssetID.ToString()), EDC_Asset, EDV_Error);
	return LoadedAsset;
}

FUniqueAssetData* FAssetManager::LoadAssetByCondition(const TFunction<bool(FUniqueAssetData*)>& InCondition)
{
	for (auto& Iter : AssetMap)
	{
		if (InCondition(Iter.Value))
		{
			return Iter.Value;
		}
	}
	return nullptr;
}

TArray<FUniqueAssetData*> FAssetManager::LoadAssetsByType(FUniqueType InAssetType)
{
	TArray<FUniqueAssetData*> LoadedAssets;
	for (auto& Iter : AssetMap)
	{
		if (Iter.Value->GetType().IsA(InAssetType))
		{
			LoadedAssets.Add(Iter.Value);
		}
	}
	return LoadedAssets;
}

TArray<FUniqueAssetData*> FAssetManager::LoadAssetsByCondition(const TFunction<bool(FUniqueAssetData*)>& InCondition)
{
	TArray<FUniqueAssetData*> LoadedAssets;
	for (auto& Iter : AssetMap)
	{
		if (InCondition(Iter.Value))
		{
			LoadedAssets.Add(Iter.Value);
		}
	}
	return LoadedAssets;
}

#if WITH_ENGINE
UTexture2D* FAssetManager::LoadTextureByPath(const FString& InTexturePath, bool bEnsured)
{
	UTexture2D* Texture;

	if (!TextureMap.Contains(InTexturePath))
	{
		Texture = FCoreStatics::LoadTextureFromFile(InTexturePath);
		if (Texture)
		{
			TextureMap.Add(InTexturePath, Texture);
		}
	}
	else
	{
		Texture = TextureMap[InTexturePath];
	}

	ensureEditorMsgf(!bEnsured || Texture, FString::Printf(TEXT("Failed to load texture for path %s!"), *InTexturePath), EDC_Asset, EDV_Error);
	return Texture;
}
#endif

void FAssetManager::ReleaseRuntimeData()
{
	for (auto& Iter : AssetMap)
	{
		Iter.Value->ResetRuntimeData();
	}
}
