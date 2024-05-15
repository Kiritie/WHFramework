﻿// Fill out your copyright notice in the Description page of Project Settings.

#include "Asset/Modifier/Material/AssetModifier_RemoveMaterialUserData.h"

UAssetModifier_RemoveMaterialUserData::UAssetModifier_RemoveMaterialUserData()
{
	UserDataClass = nullptr;
}

bool UAssetModifier_RemoveMaterialUserData::CanModify_Implementation(const FAssetData& InAssetData) const
{
	if(UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(InAssetData.GetAsset()))
	{
		if(UserDataClass && MaterialInterface->GetAssetUserDataOfClass(UserDataClass))
		{
			return true;
		}
	}
	return false;
}

void UAssetModifier_RemoveMaterialUserData::DoModify_Implementation(const FAssetData& InAssetData)
{
	if(UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(InAssetData.GetAsset()))
	{
		MaterialInterface->RemoveUserDataOfClass(UserDataClass);
		
		Super::DoModify_Implementation(InAssetData);
	}
}
