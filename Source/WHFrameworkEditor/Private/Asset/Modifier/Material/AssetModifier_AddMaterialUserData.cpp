// Fill out your copyright notice in the Description page of Project Settings.

#include "Asset/Modifier/Material/AssetModifier_AddMaterialUserData.h"

UAssetModifier_AddMaterialUserData::UAssetModifier_AddMaterialUserData()
{
	UserDataClass = nullptr;
}

bool UAssetModifier_AddMaterialUserData::CanModify_Implementation(const FAssetData& InAssetData) const
{
	if(UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(InAssetData.GetAsset()))
	{
		if(UserDataClass && !MaterialInterface->GetAssetUserDataOfClass(UserDataClass))
		{
			return true;
		}
	}
	return false;
}

void UAssetModifier_AddMaterialUserData::DoModify_Implementation(const FAssetData& InAssetData)
{
	if(UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(InAssetData.GetAsset()))
	{
		UAssetUserData* UserData = NewObject<UAssetUserData>(MaterialInterface, UserDataClass);
		MaterialInterface->AddAssetUserData(UserData);
		
		Super::DoModify_Implementation(InAssetData);
	}
}
