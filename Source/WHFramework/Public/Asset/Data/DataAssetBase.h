// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
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
	UFUNCTION(BlueprintNativeEvent)
	void InitData();

	UFUNCTION(BlueprintNativeEvent)
	void FillData(const FString& InJsonData);

	UFUNCTION(BlueprintNativeEvent)
	FString PackData();

protected:
	UPROPERTY(EditDefaultsOnly)
	FName DataAssetName;
	
public:
	UFUNCTION(BlueprintPure)
	FName GetDataAssetName() const { return DataAssetName; }
};
