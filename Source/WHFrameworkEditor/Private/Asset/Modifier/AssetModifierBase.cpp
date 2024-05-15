// Fill out your copyright notice in the Description page of Project Settings.

#include "Asset/Modifier/AssetModifierBase.h"

UAssetModifierBase::UAssetModifierBase()
{
}

bool UAssetModifierBase::CanModify_Implementation(const FAssetData& InAssetData) const
{
	return false;
}

void UAssetModifierBase::DoModify_Implementation(const FAssetData& InAssetData)
{
	if(UObject* Asset = InAssetData.GetAsset())
	{
		Asset->Modify();
	}
}
