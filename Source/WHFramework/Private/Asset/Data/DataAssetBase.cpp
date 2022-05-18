// Fill out your copyright notice in the Description page of Project Settings.


#include "Asset/Data/DataAssetBase.h"

UDataAssetBase::UDataAssetBase()
{
	
}

void UDataAssetBase::FillData(TSharedPtr<FJsonObject> InJsonObject)
{
}

TSharedPtr<FJsonObject> UDataAssetBase::PackData()
{
	return nullptr;
}