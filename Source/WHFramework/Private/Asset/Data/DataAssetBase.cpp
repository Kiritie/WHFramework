// Fill out your copyright notice in the Description page of Project Settings.


#include "Asset/Data/DataAssetBase.h"

#include "Common/CommonBPLibrary.h"

UDataAssetBase::UDataAssetBase()
{
	
}

void UDataAssetBase::FillData(const FString& InJsonString)
{
	TSharedPtr<FJsonObject> JsonObject;
	UCommonBPLibrary::ParseJsonObjectFromString(InJsonString, JsonObject);
	FillData(InJsonString);
}

void UDataAssetBase::FillData(TSharedPtr<FJsonObject> InJsonObject)
{
	
}

FString UDataAssetBase::PackData()
{
	return TEXT("");
}