// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "Dom/JsonObject.h"
#include "DataAssetBase.generated.h"

/**
 * 
 */
UCLASS(BlueprintType, Blueprintable)
class WHFRAMEWORK_API UDataAssetBase : public UDataAsset
{
	GENERATED_BODY()

public:
	UDataAssetBase();

public:
	UFUNCTION(BlueprintNativeEvent)
	void Parse(const FString& InParams);
	
	UFUNCTION(BlueprintNativeEvent)
	FString Pack();

protected:
	UPROPERTY(EditDefaultsOnly)
	FGameplayTag DataAssetTag;
	
public:
	UFUNCTION(BlueprintPure)
	FGameplayTag GetDataAssetTag() const { return DataAssetTag; }
};
