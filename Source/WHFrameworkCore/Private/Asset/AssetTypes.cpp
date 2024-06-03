// Copyright Epic Games, Inc. All Rights Reserved.

#include "Asset/AssetTypes.h"

#include "Asset/AssetManager.h"

//////////////////////////////////////////////////////////////////////////
// GUID
namespace VigmaPrivate
{
	FGuid FGUIDHelper::EmptyGUID = FGuid();

	TMap<FName, FGuid> FGUIDHelper::GUIDMap = TMap<FName, FGuid>();

	FGuid FGUIDHelper::NewGUID(const FName& Name)
	{
		if(Name.IsNone()) return FGuid::NewGuid();
		if(!GUIDMap.Contains(Name))
		{
			GUIDMap.Add(Name, FGuid::NewGuid());
		}
		return GUIDMap[Name];
	}
}

//////////////////////////////////////////////////////////////////////////
// FVigmaAssetID
FUniqueAssetID FUniqueAssetID::EmptyObjectKey = FObjectKey();
FUniqueAssetID FUniqueAssetID::EmptyGuid = FGuid();
FUniqueAssetID FUniqueAssetID::EmptyName = FName();
FUniqueAssetID FUniqueAssetID::EmptyUniqueID = FUniqueID(0);

//////////////////////////////////////////////////////////////////////////
// FUniqueAssetData
const FUniqueType FUniqueAssetData::Type = FUniqueType();

//////////////////////////////////////////////////////////////////////////
// FUniqueAssetItem
const FUniqueType FUniqueAssetItem::Type = FUniqueType();

FUniqueAssetData* FUniqueAssetItem::GetAssetData() const
{
	return FAssetManager::Get().LoadAssetByID(AssetID);
}
