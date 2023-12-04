// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/Asset/AssetDefinitionBase.h"


#define LOCTEXT_NAMESPACE "AssetTypeActions"

UAssetDefinitionBase::UAssetDefinitionBase()
{
	AssetDisplayName = NSLOCTEXT("AssetTypeActions", "AssetTypeActions_AssetBase", "Asset Base");
	AssetCategories = { LOCTEXT("WHFramework", "WHFramework") };
	AssetColor = FLinearColor(FColor(62, 140, 35));
	AssetClass = UObject::StaticClass();
}

#undef LOCTEXT_NAMESPACE
