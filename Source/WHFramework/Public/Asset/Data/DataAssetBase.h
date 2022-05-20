// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Dom/JsonObject.h"
#include "DataAssetBase.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UDataAssetBase : public UDataAsset
{
	GENERATED_BODY()

public:
	UDataAssetBase();

public:
	virtual void FillData(const FString& InJsonString);

	virtual void FillData(TSharedPtr<FJsonObject> InJsonObject);
	
	virtual FString PackData();

protected:
	UPROPERTY(EditDefaultsOnly)
	FName DataAssetName;
	
public:
	UFUNCTION(BlueprintPure)
	FName GetDataAssetName() const { return DataAssetName; }
};
