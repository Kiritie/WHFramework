// Fill out your copyright notice in the Description page of Project Settings.

#include "Asset/Modifier/Material/AssetModifier_AddAssetUserData.h"

UAssetModifier_AddAssetUserData::UAssetModifier_AddAssetUserData()
{
	UserDataClass = nullptr;
}

bool UAssetModifier_AddAssetUserData::CanModify_Implementation(const FAssetData& InAssetData) const
{
	if(IInterface_AssetUserData* AssetInterface = Cast<IInterface_AssetUserData>(InAssetData.GetAsset()))
	{
		if(UserDataClass && !AssetInterface->GetAssetUserDataOfClass(UserDataClass))
		{
			return true;
		}
	}
	return false;
}

void UAssetModifier_AddAssetUserData::DoModify_Implementation(const FAssetData& InAssetData)
{
	if(IInterface_AssetUserData* AssetInterface = Cast<IInterface_AssetUserData>(InAssetData.GetAsset()))
	{
		UAssetUserData* UserData = NewObject<UAssetUserData>(InAssetData.GetAsset(), UserDataClass);
		AssetInterface->AddAssetUserData(UserData);
		
		Super::DoModify_Implementation(InAssetData);
	}
}
