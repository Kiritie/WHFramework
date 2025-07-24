// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypes.h"
#include "Main/MainTypes.h"
#include "Main/Base/ManagerBase.h"

class WHFRAMEWORKCORE_API FAssetManager : public FManagerBase
{
	GENERATED_MANAGER(FAssetManager)

public:
	FAssetManager();
	
	static FUniqueType Type;

public:
	virtual void OnInitialize() override;

public:
	virtual bool RegisterAsset(FUniqueAssetData* InAssetData);
	
	virtual void RegisterAssets(TArray<FUniqueAssetData*> InAssetDatas);

	template<class T>
	void RegisterAssets(TArray<T*> InAssetDatas)
	{
		for(const auto Iter : InAssetDatas)
		{
			RegisterAsset(Iter);
		}
	}

	virtual bool UnRegisterAssetByID(const FUniqueAssetID& InAssetID);

	virtual bool UnRegisterAsset(FUniqueAssetData* InAssetData);
	
	virtual void UnRegisterAssets(TArray<FUniqueAssetData*> InAssetDatas);

	template<class T>
	void UnRegisterAssets(TArray<T*> InAssetDatas)
	{
		for(const auto Iter : InAssetDatas)
		{
			UnRegisterAssetByID(Iter);
		}
	}

	template<class T>
	void UnRegisterAssetsByType(FUniqueType InAssetType = T::Type)
	{
		TArray<FUniqueAssetID> UnRegisterAssets;
		for(auto& Iter : AssetMap)
		{
			if (Iter.Value->GetClassType().IsA(InAssetType))
			{
				UnRegisterAssets.Add(Iter.Key);
			}
		}
		for(auto& Iter : UnRegisterAssets)
		{
			UnRegisterAssetByID(Iter);
		}
	}

	virtual bool ContainsAssetByID(const FUniqueAssetID& InAssetID) const;

	virtual bool ContainsAssetByCondition(const TFunction<bool(FUniqueAssetData*)>& InCondition) const;
	
	template<class T>
	bool ContainsAssetByCondition(const TFunction<bool(T*)>& InCondition) const
	{
		for (auto& Iter : AssetMap)
		{
			if (Iter.Value->IsA<T>())
			{
				if (InCondition(Iter.Value->Cast<T>()))
				{
					return true;
				}
			}
		}
		return false;
	}

	virtual bool ContainsAssetsByType(FUniqueType InAssetType) const;
	
	template<class T>
	bool ContainsAssetsByType(FUniqueType InAssetType = T::Type)
	{
		return ContainsAssetsByType(InAssetType);
	}

	virtual FUniqueAssetData* LoadAssetByID(const FUniqueAssetID& InAssetID, bool bEnsured = true);

	template<class T>
	T* LoadAssetByID(const FUniqueAssetID& InAssetID, bool bEnsured = true)
	{
		if(FUniqueAssetData* LoadedAsset = LoadAssetByID(InAssetID, bEnsured))
		{
			return LoadedAsset->Cast<T>();
		}
		return nullptr;
	}

	virtual FUniqueAssetData* LoadAssetByCondition(const TFunction<bool(FUniqueAssetData*)>& InCondition);

	template<class T>
	T* LoadAssetByCondition(const TFunction<bool(T*)>& InCondition)
	{
		for(auto& Iter : AssetMap)
		{
			if(T* LoadedAsset = Iter.Value->Cast<T>())
			{
				if(InCondition(LoadedAsset))
				{
					return LoadedAsset;
				}
			}
		}
		return nullptr;
	}

	virtual TArray<FUniqueAssetData*> LoadAssetsByType(FUniqueType InAssetType);

	template<class T>
	TArray<T*> LoadAssetsByType(FUniqueType InAssetType = T::Type)
	{
		TArray<T*> LoadedAssets;
		for(auto& Iter : AssetMap)
		{
			if (Iter.Value->GetClassType().IsA(InAssetType))
			{
				if(T* LoadedAsset = Iter.Value->Cast<T>())
				{
					LoadedAssets.Add(LoadedAsset);
				}
			}
		}
		return LoadedAssets;
	}

	virtual TArray<FUniqueAssetData*> LoadAssetsByCondition(const TFunction<bool(FUniqueAssetData*)>& InCondition);

	template<class T>
	TArray<T*> LoadAssetsByCondition(const TFunction<bool(T*)>& InCondition)
	{
		TArray<T*> LoadedAssets;
		for(auto& Iter : AssetMap)
		{
			if(T* LoadedAsset = Iter.Value->Cast<T>())
			{
				if(InCondition(LoadedAsset))
				{
					LoadedAssets.Add(LoadedAsset);
				}
			}
		}
		return LoadedAssets;
	}

	virtual UTexture2D* LoadTextureByPath(const FString& InTexturePath, bool bEnsured = true);

	virtual void ReleaseRuntimeData();

protected:
	TMap<FUniqueAssetID, FUniqueAssetData*> AssetMap;

	TMap<FString, UTexture2D*> TextureMap;
};
